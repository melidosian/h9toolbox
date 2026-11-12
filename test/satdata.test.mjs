import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import { appState, withApp } from '../dist/apps/catalog.mjs';
import { MAX_SATELLITES, describe, fetchTles, pack, parseTles } from '../dist/apps/satdata.mjs';

const fixture = name => readFileSync(new URL(`./fixtures/${name}`, import.meta.url));
const tles = parseTles(fixture('satpass-default.tle').toString('utf8'));
const { satellites } = JSON.parse(fixture('satpass-satellites.json').toString('utf8'));

test('the browser packer produces the same bytes as satpack.py', () => {
  const { body, info } = pack(tles, satellites);
  assert.deepEqual(Buffer.from(body), fixture('satpass-satdata.bin'));
  assert.equal(info.count, 8);
  assert.equal(info.date, '2026-11-10');
});

test('a subset keeps the order and describes itself', () => {
  const chosen = satellites.filter(s => ['iss', 'ao91'].includes(s.id));
  const { body } = pack(tles, chosen);
  assert.equal(body.length, 32 + 64 * chosen.length);
  assert.deepEqual(describe(body).labels, chosen.map(s => s.label));
});

test('bad requests are refused with readable errors', () => {
  assert.throws(() => pack(tles, []), /Choose 1 to 16/);
  assert.throws(() => pack(tles, Array(MAX_SATELLITES + 1).fill(satellites[0])), /Choose 1 to 16/);
  assert.throws(() => pack(new Map(), satellites), /Not in the orbital data/);
  assert.throws(() => pack(tles, [{ ...satellites[0], label: 'TOOLONG8' }]), /at most 7/);
  assert.equal(describe(new Uint8Array(40)), null);
});

test('fetchTles reports failures and thin answers', async () => {
  await assert.rejects(fetchTles(async () => { throw new Error('offline'); }), /Could not reach CelesTrak \(offline\)/);
  await assert.rejects(fetchTles(async () => ({ ok: false, status: 503 })), /503/);
  await assert.rejects(fetchTles(async () => ({ ok: true, text: async () => 'nothing' })), /no usable/);
  const ok = await fetchTles(async () => ({ ok: true, text: async () => fixture('satpass-default.tle').toString('utf8') }), 'x');
  assert.equal(ok.size, 7);
});

test('refreshed satellite data is the user’s: it is kept on update and never looks outdated', () => {
  const item = { name: 'SAT PASS', modules: [{ name: 'SAT PASS', crc16: 'aaaa' }, { name: '.SAT DATA', crc16: 'bbbb' }], data: ['.SAT DATA'] };
  const m = (name, bytes) => ({ name, body: Uint8Array.from(bytes), bss: 0, entry: 0 });
  const menu = m('MENU', [1]);
  const fresh = { name: '.SAT DATA', body: Uint8Array.of(9, 9, 9), bss: 0, entry: 0 };
  const radio = [menu, m('SAT PASS', [1]), fresh];
  // The app code's CRC differs from the catalog (so it is outdated) but the data module's does not matter.
  assert.equal(appState({ ...item, modules: [{ name: 'SAT PASS', crc16: '0000' }, { name: '.SAT DATA', crc16: 'ffff' }] }, radio, radio).status, 'outdated');
  const catalogParts = [m('SAT PASS', [2]), m('.SAT DATA', [0])];
  const updated = withApp(radio, catalogParts, null, item);
  assert.equal(updated.find(x => x.name === 'SAT PASS').body[0], 2);
  assert.equal(updated.find(x => x.name === '.SAT DATA'), fresh);   // kept, not replaced by the built-in
  const plain = withApp(radio, catalogParts, null, { ...item, data: undefined });
  assert.equal(plain.find(x => x.name === '.SAT DATA').body[0], 0);
});

test('every published app has a short summary and the satellite app offers its settings', () => {
  const catalog = JSON.parse(readFileSync(new URL('../dist/apps/catalog/catalog.json', import.meta.url), 'utf8'));
  for (const app of catalog.apps) {
    assert.ok(app.summary && app.summary.length <= 90, `${app.id}: summary up to 90 characters`);
    assert.ok(Array.isArray(app.controls) && app.controls.length, `${app.id}: controls`);
  }
  const sat = catalog.apps.find(a => a.id === 'sat_pass');
  assert.equal(sat.config, 'satellites');
  assert.deepEqual(sat.data, ['.SAT DATA']);
  const published = JSON.parse(readFileSync(new URL('../dist/apps/catalog/satellites.json', import.meta.url), 'utf8'));
  assert.deepEqual(published, JSON.parse(fixture('satpass-satellites.json').toString('utf8')));
});

test('every screenshot a catalog entry lists is a published PNG', () => {
  const dir = new URL('../dist/apps/catalog/', import.meta.url);
  const catalog = JSON.parse(readFileSync(new URL('catalog.json', dir), 'utf8'));
  let shots = 0;
  for (const app of catalog.apps) for (const shot of app.screenshots ?? []) {
    const bytes = readFileSync(new URL(shot.file, dir));
    assert.deepEqual([...bytes.subarray(0, 8)], [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a], `${shot.file} is a PNG`);
    assert.equal(bytes.readUInt32BE(16), 480, `${shot.file} is 3x the radio's width`);
    assert.equal(bytes.readUInt32BE(20), 384);
    assert.ok(shot.caption.length > 5 && shot.caption.length < 90);
    shots++;
  }
  assert.ok(shots >= 5);
});
