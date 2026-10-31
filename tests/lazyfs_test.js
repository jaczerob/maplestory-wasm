const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const context = { Module: {}, console, Uint8Array, ArrayBuffer, DataView, TextDecoder, setTimeout: () => 0 };
vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../src/client/LazyFS/lazyfs.js'), 'utf8'), context);
const lazy = context.Module.LazyFS;
lazy.CHUNK_SIZE = 3;
lazy.dbReady = true;
lazy.logFetch = () => {};
lazy.setCachedChunk = async () => {};
lazy.registerFile('Map.nx');

let version = 'old';
let reconnect = false;
let chunkRequests = 0;
let fail = false;
let payload = [1, 1, 1];
lazy.connect = async function () {
    if (reconnect) {
        this.connectionGeneration++;
        for (const entry of this.files.values()) {
            entry.generation++;
            entry.size = null;
            entry.version = null;
        }
        reconnect = false;
    }
};
lazy.send = function (message) {
    if (message.type === 'get_size') {
        this.handleMessage(JSON.stringify({ type: 'size', file: message.file, size: 3, version }));
        return;
    }
    chunkRequests++;
    if (fail) {
        this.handleMessage(JSON.stringify({ type: 'error', file: message.file, index: message.index, message: 'Missing component' }));
        return;
    }
    const name = Buffer.from(message.file);
    const frame = Buffer.alloc(5 + name.length + payload.length);
    frame.writeUInt32LE(message.index);
    frame[4] = name.length;
    name.copy(frame, 5);
    Buffer.from(payload).copy(frame, 5 + name.length);
    this.handleBinaryChunk(Uint8Array.from(frame).buffer);
};

(async () => {
    assert.deepEqual(Array.from(await lazy.getChunk('Map.nx', 0)), [1, 1, 1]);
    assert.deepEqual(Array.from(await lazy.getChunk('Map.nx', 0)), [1, 1, 1]);
    assert.equal(chunkRequests, 1, 'a warm chunk should use the cache');

    reconnect = true;
    version = 'new';
    payload = [2, 2, 2];
    assert.deepEqual(Array.from(await lazy.getChunk('Map.nx', 0)), [2, 2, 2]);
    assert.equal(chunkRequests, 2, 'a same-size replacement must fetch new bytes after reconnect');
    assert.deepEqual(Array.from(await lazy.readFileData('Map.nx', 0, 3)), [2, 2, 2]);
    assert.equal(await lazy.readFileData('Map.nx', 3, 1), null);

    fail = true;
    await assert.rejects(lazy.getChunk('Map.nx', 1), /Missing component/);
    assert.equal(lazy.pendingRequests.size, 0);
    console.log('LazyFS cache, reconnect, range, and component-error checks passed.');
})().catch(error => {
    console.error(error);
    process.exitCode = 1;
});
