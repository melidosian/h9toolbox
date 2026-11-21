// The browser updater publishes stable firmware. RC builds are flashed separately.
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { BUILD } from '../dist/firmware-update/firmware.mjs';

const root = new URL('../', import.meta.url);
function numericVersion(version) {
  assert.match(version, /^\d{1,3}\.\d{1,3}\.\d{1,3}$/);
  const parts = version.split('.').map(Number);
  assert.ok(parts.every(part => part < 256));
  return parts.reduce((value, part) => value * 256 + part, 0);
}

test('the updater offers the stable release when the loader is a release candidate', async () => {
  const text = (await readFile(new URL('firmware-research/module-loader/VERSION', root), 'utf8')).trim();
  const [, version, candidate] = /^(\d+\.\d+\.\d+)(?:-rc\.([1-9]\d{0,2}))?$/.exec(text) ?? [];
  assert.ok(version, 'the loader version is a stable release or numbered release candidate');
  const loaderVersion = numericVersion(version);
  const publishedVersion = numericVersion(BUILD.version);
  if (candidate) {
    assert.ok(Number(candidate) < 256);
    assert.ok(publishedVersion < loaderVersion);
  } else {
    assert.equal(BUILD.version, text);
  }
  assert.ok(BUILD.label.endsWith(`v${BUILD.version}`), 'the label names the published version');
  assert.equal(BUILD.packageFile, `h9-toolbox-firmware-v${BUILD.version}.fw`);
});
