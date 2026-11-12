// The firmware version has one source, firmware-research/module-loader/VERSION (major.minor.patch, each below 256).
// The installer's BUILD must show the same version; build.py checks the built image carries it in the loader's API table
// (src/stub.s ml_fw_version, read by the ABOUT module as api->firmware).
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { BUILD } from '../dist/firmware-update/firmware.mjs';

const root = new URL('../', import.meta.url);
test('the installer build version matches module-loader/VERSION', async () => {
  const text = (await readFile(new URL('firmware-research/module-loader/VERSION', root), 'utf8')).trim();
  assert.match(text, /^\d{1,3}\.\d{1,3}\.\d{1,3}$/);
  const [major, minor, patch] = text.split('.').map(Number);
  assert.ok(major < 256 && minor < 256 && patch < 256);
  assert.equal(BUILD.version, text);
  assert.ok(BUILD.label.endsWith(`v${text}`), 'the label names the version');
});
