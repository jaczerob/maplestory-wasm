import asyncio
import hashlib
import importlib.util
import json
import tempfile
import time
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("assets_server.py")
SPEC = importlib.util.spec_from_file_location("assets_server", MODULE_PATH)
assets_server = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(assets_server)


class AssetServerTests(unittest.TestCase):
    def test_canonical_json_escapes_actual_separators_but_preserves_literal_escape_text(self):
        value = {"text": "actual\u2028/literal\\u2028/<> &"}
        self.assertEqual(assets_server.canonical_json(value), b'{"text":"actual\\u2028/literal\\\\u2028/<> &"}')

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        (self.root / "shared").mkdir()
        (self.root / "client").mkdir()
        metadata, media = b"metadata", b"payload"
        (self.root / "shared" / "Map.meta").write_bytes(metadata)
        (self.root / "client" / "Map.media").write_bytes(media)
        self.write_manifest(metadata, media)

    def tearDown(self):
        self.temp.cleanup()

    def write_manifest(self, metadata, media):
        manifest = {
            "schema_version": 1,
            "catalog_version": "test",
            "archives": {"Map.nx": {
                "metadata_bytes": len(metadata),
                "media_bytes": len(media),
                "metadata_sha256": hashlib.sha256(metadata).hexdigest(),
                "media_sha256": hashlib.sha256(media).hexdigest(),
            }},
            "catalog_sha256": {},
            "provenance": {},
        }
        manifest["source_hash"] = hashlib.sha256(assets_server.canonical_json(manifest)).hexdigest()
        (self.root / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")

    def test_reads_across_metadata_and_media_boundary_and_uses_content_version(self):
        server = assets_server.AssetServer(self.root)
        self.assertEqual(server.read_range("Map.nx", 6, 5), b"tapay")
        self.assertEqual(server.get_file_size("Map.nx"), 15)
        self.assertEqual(server.read_range("Map.nx", 0, 15), b"metadatapayload")
        self.assertEqual(server.get_file_version("Map.nx"), server.get_file_version("Map.nx"))

    def test_same_size_replacement_changes_content_version(self):
        old_version = assets_server.AssetServer(self.root).get_file_version("Map.nx")
        (self.root / "shared" / "Map.meta").write_bytes(b"metadAtA")
        self.write_manifest(b"metadAtA", b"payload")
        new_version = assets_server.AssetServer(self.root).get_file_version("Map.nx")
        self.assertNotEqual(old_version, new_version)

    def test_rejects_non_manifest_paths_and_corrupt_components(self):
        server = assets_server.AssetServer(self.root)
        with self.assertRaisesRegex(ValueError, "Unknown logical archive"):
            server.get_file_size("../Map.nx")
        (self.root / "shared" / "Map.meta").write_bytes(b"tampered")
        with self.assertRaisesRegex(ValueError, "hash mismatch"):
            server.read_range("Map.nx", 0, 1)

    def test_rejects_manifest_source_hash_mismatch(self):
        manifest = json.loads((self.root / "manifest.json").read_text(encoding="utf-8"))
        manifest["catalog_version"] = "changed"
        (self.root / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "source_hash mismatch"):
            assets_server.AssetServer(self.root)

    def test_missing_component_error_is_correlated_to_chunk_request(self):
        class Socket:
            def __init__(self):
                self.messages = []

            async def send(self, message):
                self.messages.append(message)

        server = assets_server.AssetServer(self.root)
        (self.root / "client" / "Map.media").unlink()
        socket = Socket()
        asyncio.run(server.handle_message(socket, json.dumps({
            "type": "get_chunk", "file": "Map.nx", "index": 3, "chunk_size": 8
        })))
        response = json.loads(socket.messages[0])
        self.assertEqual(response["type"], "error")
        self.assertEqual(response["file"], "Map.nx")
        self.assertEqual(response["index"], 3)
        self.assertIn("Missing component", response["message"])

    def test_slow_first_validation_is_offloaded_and_shared(self):
        class Socket:
            async def send(self, message):
                pass

        server = assets_server.AssetServer(self.root)
        validate = server._validate
        calls = []

        def slow_validate(filename):
            calls.append(filename)
            time.sleep(0.15)
            return validate(filename)

        server._validate = slow_validate

        async def exercise():
            message = json.dumps({"type": "get_size", "file": "Map.nx"})
            started = time.monotonic()
            first = asyncio.create_task(server.handle_message(Socket(), message))
            await asyncio.sleep(0.01)
            self.assertLess(time.monotonic() - started, 0.1)
            second = asyncio.create_task(server.handle_message(Socket(), message))
            await asyncio.wait_for(asyncio.gather(first, second), timeout=1)

        asyncio.run(exercise())
        self.assertEqual(calls, ["Map.nx"])

    def test_allows_omitted_empty_media(self):
        metadata = b"metadata"
        (self.root / "shared" / "Map.meta").write_bytes(metadata)
        (self.root / "client" / "Map.media").unlink()
        self.write_manifest(metadata, b"")
        server = assets_server.AssetServer(self.root)
        self.assertEqual(server.get_file_size("Map.nx"), len(metadata))
        self.assertEqual(server.read_range("Map.nx", 0, len(metadata)), metadata)

    def test_rejects_non_object_requests_without_closing_connection(self):
        class Socket:
            async def send(self, message):
                self.message = json.loads(message)

        server = assets_server.AssetServer(self.root)
        socket = Socket()
        for request in ([], None, "invalid"):
            asyncio.run(server.handle_message(socket, json.dumps(request)))
            self.assertEqual(socket.message["type"], "error")
            self.assertIn("JSON object", socket.message["message"])


if __name__ == "__main__":
    unittest.main()
