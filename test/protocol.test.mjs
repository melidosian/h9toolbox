import test from 'node:test';
import assert from 'node:assert/strict';
import { readBlockReply } from '../dist/protocol.mjs';

function stream(bytes, optional = null) {
  const queue = [...bytes];
  return {
    readExact: async length => {
      assert.ok(queue.length >= length, 'reply must contain enough bytes');
      return Uint8Array.from(queue.splice(0, length));
    },
    readOptionalByte: async () => optional === null ? null : queue.shift(),
    remaining: () => queue,
  };
}

const data = Uint8Array.from({ length: 32 }, (_, i) => i);
const header = address => [0x57, address >> 8, address & 255, 0x20];

test('auto-detects 36-byte reply without checksum', async () => {
  const input = stream([...header(0), ...data]);
  const reply = await readBlockReply(0, 32, 'auto', input.readExact, input.readOptionalByte);
  assert.equal(reply.mode, 'no');
  assert.deepEqual(reply.data, data);
  assert.deepEqual(input.remaining(), []);
});

test('auto-detects 37-byte reply and consumes checksum', async () => {
  const input = stream([...header(0), ...data, 0x86], true);
  const reply = await readBlockReply(0, 32, 'auto', input.readExact, input.readOptionalByte);
  assert.equal(reply.mode, 'yes');
  assert.deepEqual(reply.data, data);
  assert.deepEqual(input.remaining(), []);
});

test('resynchronizes a late checksum before the next valid header', async () => {
  const input = stream([0x57, ...header(0x20), ...data, 0x86]);
  const reply = await readBlockReply(0x20, 32, 'no', input.readExact, input.readOptionalByte);
  assert.equal(reply.mode, 'yes');
  assert.deepEqual(reply.data, data);
  assert.deepEqual(input.remaining(), []);
});

test('rejects a reply for the wrong block address', async () => {
  const input = stream([...header(0x40), ...data]);
  await assert.rejects(readBlockReply(0x20, 32, 'no', input.readExact, input.readOptionalByte), /Invalid read header/);
});
