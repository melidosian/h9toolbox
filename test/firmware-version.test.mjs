// The browser updater can stage a new release on the development site.
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { quickChangeBuild } from '../dist/firmware-update/combined-builds.mjs';
import { FAR_START, FAR_END } from '../dist/firmware-update/far-region.mjs';
import { BUILD, decodePackage, sha256 } from '../dist/firmware-update/firmware.mjs';

const root = new URL('../', import.meta.url);
function numericVersion(version) {
  assert.match(version, /^\d{1,3}\.\d{1,3}\.\d{1,3}$/);
  const parts = version.split('.').map(Number);
  assert.ok(parts.every(part => part < 256));
  return parts.reduce((value, part) => value * 256 + part, 0);
}

test('the loader version is represented by the staged or published installer', async () => {
  const text = (await readFile(new URL('firmware-research/module-loader/VERSION', root), 'utf8')).trim();
  const [, version, candidate] = /^(\d+\.\d+\.\d+)(?:-rc\.([1-9]\d{0,2}))?$/.exec(text) ?? [];
  assert.ok(version, 'the loader version is a stable release or numbered release candidate');
  const loaderVersion = numericVersion(version);
  const publishedVersion = numericVersion(BUILD.version);
  if (candidate) {
    assert.ok(Number(candidate) < 256);
    assert.ok(publishedVersion < loaderVersion);
  } else {
    assert.ok(publishedVersion <= loaderVersion);
    if (publishedVersion < loaderVersion) assert.equal(quickChangeBuild.version, text);
  }
  assert.ok(BUILD.label.endsWith(`v${BUILD.version}`), 'the label names the published version');
  assert.equal(BUILD.packageFile, `h9-toolbox-firmware-v${BUILD.version}.fw`);
});

test('the staged installer matches both v0.1.7 firmware regions', async () => {
  const release = new URL(`firmware-research/module-loader/releases/${quickChangeBuild.version}/`, root);
  const load = async file => new Uint8Array(await readFile(new URL(file.split('/').pop(), release)));
  const [pack, far] = await Promise.all([load(quickChangeBuild.file), load(quickChangeBuild.farFile)]);
  assert.equal(await sha256(pack), quickChangeBuild.packageSha256);
  assert.equal(await sha256(decodePackage(pack)), quickChangeBuild.imageSha256);
  assert.equal(await sha256(far), quickChangeBuild.farSha256);
  assert.equal(far.length, FAR_END - FAR_START);
  const catalog = JSON.parse(await readFile(new URL('apps/catalog.json', release), 'utf8'));
  assert.ok(catalog.menu.notes.includes(`Use matching H9 Toolbox Firmware v${quickChangeBuild.version}.`));
});
