#!/usr/bin/env python3
"""Serve virtual NX files assembled from validated metadata and media sidecars."""

import argparse
import asyncio
import hashlib
import json
from pathlib import Path
import re

try:
    import websockets
except ImportError:
    print("Please install websockets: pip install websockets")
    raise SystemExit(1)

DEFAULT_PORT = 8765
DEFAULT_DIRECTORY = "."
CHUNK_SIZE = 2 * 1024 * 1024
MAX_BATCH_CHUNKS = 128
EMPTY_SHA256 = hashlib.sha256(b"").hexdigest()


def canonical_json(value):
    encoded = json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
    return encoded.replace("\u2028", "\\u2028").replace("\u2029", "\\u2029").encode("utf-8")


def stream_hash(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


class AssetServer:
    def __init__(self, directory):
        self.directory = Path(directory).resolve()
        self.manifest = self._load_manifest()
        self.archives = self.manifest["archives"]
        self.validated = set()
        self.validation_tasks = {}
        print(f"[AssetServer] Serving assets from: {self.directory}")

    def _load_manifest(self):
        path = self.directory / "manifest.json"
        try:
            manifest = json.loads(path.read_text(encoding="utf-8"))
            if manifest.get("schema_version") != 1 or not isinstance(manifest.get("archives"), dict):
                raise ValueError("unsupported manifest schema")
            projection = {key: manifest[key] for key in
                          ("schema_version", "catalog_version", "archives", "catalog_sha256", "provenance")}
            if hashlib.sha256(canonical_json(projection)).hexdigest() != manifest.get("source_hash"):
                raise ValueError("manifest source_hash mismatch")
            for name, entry in manifest["archives"].items():
                if not isinstance(name, str) or not re.fullmatch(r"[A-Za-z0-9_.-]+\.nx", name) or Path(name).name != name:
                    raise ValueError(f"invalid archive name: {name!r}")
                if any(not isinstance(entry.get(field), int) or entry[field] < 0 for field in ("metadata_bytes", "media_bytes")):
                    raise ValueError(f"invalid component size for {name}")
                if any(not isinstance(entry.get(field), str) or len(entry[field]) != 64
                       for field in ("metadata_sha256", "media_sha256")):
                    raise ValueError(f"invalid component hash for {name}")
            return manifest
        except (OSError, KeyError, TypeError, json.JSONDecodeError, ValueError) as error:
            raise RuntimeError(f"Invalid asset manifest {path}: {error}") from error

    def _paths(self, filename):
        entry = self.archives.get(filename)
        if entry is None:
            raise ValueError(f"Unknown logical archive: {filename}")
        stem = filename[:-3]
        return entry, self.directory / "shared" / f"{stem}.meta", self.directory / "client" / f"{stem}.media"

    def _validate(self, filename):
        if filename in self.validated:
            return self._paths(filename)
        entry, metadata, media = self._paths(filename)
        for path, size_key, hash_key in ((metadata, "metadata_bytes", "metadata_sha256"),
                                         (media, "media_bytes", "media_sha256")):
            expected_size = entry[size_key]
            if expected_size == 0 and path == media and not path.exists():
                actual_hash = EMPTY_SHA256
            else:
                if not path.is_file():
                    raise ValueError(f"Missing component for {filename}: {path.name}")
                actual_size = path.stat().st_size
                if actual_size != expected_size:
                    raise ValueError(f"Truncated or oversized component for {filename}: {path.name} expected {expected_size}, got {actual_size}")
                actual_hash = stream_hash(path)
            if actual_hash != entry[hash_key]:
                raise ValueError(f"Component hash mismatch for {filename}: {path.name}")
        self.validated.add(filename)
        return entry, metadata, media

    def get_file_size(self, filename):
        entry, _, _ = self._validate(filename)
        return entry["metadata_bytes"] + entry["media_bytes"]

    async def _validate_async(self, filename):
        if filename in self.validated:
            return self._paths(filename)
        self._paths(filename)
        task = self.validation_tasks.get(filename)
        if task is None:
            task = asyncio.create_task(asyncio.to_thread(self._validate, filename))
            self.validation_tasks[filename] = task
        try:
            return await asyncio.shield(task)
        finally:
            if task.done() and self.validation_tasks.get(filename) is task:
                del self.validation_tasks[filename]

    async def get_file_size_async(self, filename):
        entry, _, _ = await self._validate_async(filename)
        return entry["metadata_bytes"] + entry["media_bytes"]

    def get_file_version(self, filename):
        entry = self.archives.get(filename)
        if entry is None:
            raise ValueError(f"Unknown logical archive: {filename}")
        return hashlib.sha256((entry["metadata_sha256"] + entry["media_sha256"]).encode("ascii")).hexdigest()

    def read_range(self, filename, offset, length):
        entry, metadata, media = self._validate(filename)
        total = entry["metadata_bytes"] + entry["media_bytes"]
        if offset < 0 or length < 0 or offset > total or length > total - offset:
            raise ValueError(f"Range outside {filename}: offset={offset}, length={length}, size={total}")
        expected = length
        result = bytearray()
        if offset < entry["metadata_bytes"]:
            count = min(length, entry["metadata_bytes"] - offset)
            with metadata.open("rb") as source:
                source.seek(offset)
                result.extend(source.read(count))
            offset += count
            length -= count
        if length:
            media_offset = offset - entry["metadata_bytes"]
            with media.open("rb") as source:
                source.seek(media_offset)
                result.extend(source.read(length))
        if len(result) != expected:
            raise ValueError(f"Short read for {filename}")
        return bytes(result)

    def read_chunk(self, filename, chunk_index, chunk_size):
        if not isinstance(chunk_index, int) or chunk_index < 0 or chunk_index > 0xFFFFFFFF:
            raise ValueError("chunk index must be an unsigned 32-bit integer")
        if not isinstance(chunk_size, int) or not 0 < chunk_size <= CHUNK_SIZE:
            raise ValueError(f"chunk_size must be between 1 and {CHUNK_SIZE}")
        size = self.get_file_size(filename)
        start = chunk_index * chunk_size
        if start >= size:
            raise ValueError(f"Chunk outside {filename}: index={chunk_index}, size={size}")
        return self.read_range(filename, start, min(chunk_size, max(0, size - start)))

    async def _error(self, websocket, filename, message, **correlation):
        await websocket.send(json.dumps({"type": "error", "file": filename, "message": str(message), **correlation}))

    async def handle_message(self, websocket, message):
        filename = None
        data = {}
        try:
            request = json.loads(message)
            if not isinstance(request, dict):
                raise ValueError("message must be a JSON object")
            data = request
            filename = data.get("file")
            if not isinstance(filename, str):
                raise ValueError("file must be a logical archive name")
            msg_type = data.get("type")
            if msg_type == "get_size":
                await websocket.send(json.dumps({"type": "size", "file": filename,
                                                 "size": await self.get_file_size_async(filename),
                                                 "version": self.get_file_version(filename)}))
            elif msg_type in ("get_chunks", "get_chunk"):
                chunk_size = data.get("chunk_size", CHUNK_SIZE)
                start = data.get("start", data.get("index", 0))
                end = data.get("end", start)
                if not isinstance(start, int) or not isinstance(end, int) or end < start or end - start + 1 > MAX_BATCH_CHUNKS:
                    raise ValueError(f"chunk range must contain 1 to {MAX_BATCH_CHUNKS} chunks")
                await self._validate_async(filename)
                name = filename.encode("utf-8")
                if len(name) > 255:
                    raise ValueError("logical archive name is too long")
                for index in range(start, end + 1):
                    chunk = self.read_chunk(filename, index, chunk_size)
                    if chunk:
                        await websocket.send(index.to_bytes(4, "little") + bytes((len(name),)) + name + chunk)
                if msg_type == "get_chunks":
                    await websocket.send(json.dumps({"type": "chunks_done", "file": filename,
                                                     "start": start, "end": end}))
            else:
                raise ValueError(f"Unknown message type: {msg_type}")
        except json.JSONDecodeError as error:
            await self._error(websocket, filename, f"Invalid JSON: {error}")
        except Exception as error:
            correlation = {}
            if data.get("type") == "get_chunk":
                correlation["index"] = data.get("index", 0)
            elif data.get("type") == "get_chunks":
                correlation.update(start=data.get("start", 0), end=data.get("end", data.get("start", 0)))
            await self._error(websocket, filename, error, **correlation)

    async def handler(self, websocket):
        async for message in websocket:
            await self.handle_message(websocket, message)


async def main(port, directory):
    server = AssetServer(directory)
    print(f"[AssetServer] Starting WebSocket server on port {port}")
    async with websockets.serve(server.handler, "0.0.0.0", port, max_size=50 * 1024 * 1024, compression=None):
        await asyncio.Future()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="WebSocket Asset Server")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--directory", type=str, default=DEFAULT_DIRECTORY)
    args = parser.parse_args()
    try:
        asyncio.run(main(args.port, args.directory))
    except KeyboardInterrupt:
        print("\n[AssetServer] Shutting down...")
