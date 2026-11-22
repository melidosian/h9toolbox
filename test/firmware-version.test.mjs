import test from 'node:test';
import assert from 'node:assert/strict';
import { execFile } from 'node:child_process';
import { cp, mkdir, mkdtemp, readFile, rm, stat } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { promisify } from 'node:util';
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

test('the updater retains the pinned v0.1.5 recovery package', async () => {
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

test('the GitHub Pages build publishes v0.1.6 as the default firmware with its app catalog', async t => {
  const workflow = await readFile(new URL('.github/workflows/pages.yml', root), 'utf8');
  assert.match(workflow, /run: bash scripts\/build-site\.sh/);
  const site = await mkdtemp(join(tmpdir(), 'h9-pages-'));
  t.after(() => rm(site, { recursive: true, force: true }));
  for (const dir of ['dist', 'scripts', 'prototypes/kiss-tnc',
    'firmware-research/module-loader/smartbeacon-release',
    'firmware-research/module-loader/combined-preview-release']) {
    await mkdir(join(site, dir), { recursive: true });
    await cp(new URL(`${dir}/`, root), join(site, dir), { recursive: true });
  }
  await promisify(execFile)('bash', ['scripts/build-site.sh'], { cwd: site });
  const updater = await readFile(join(site, 'dist/firmware-update/index.html'), 'utf8');
  assert.match(updater, /id="optBuild" data-release-firmware/);
  assert.match(updater, /Load H9 Toolbox v0\.1\.6/);
  assert.doesNotMatch(updater, /data-next-firmware|data-legacy-dev-firmware|Load H9 Toolbox v0\.1\.5/);
  const apps = await readFile(join(site, 'dist/apps/index.html'), 'utf8');
  assert.match(apps, /id="catalog" data-release-catalog/);
  assert.doesNotMatch(apps, /data-next-catalog|data-legacy-dev-catalog/);
  const catalog = JSON.parse(await readFile(join(site, 'dist/apps/catalog/rc5/catalog.json'), 'utf8'));
  assert.ok(catalog.apps.some(app => app.id === 'aprs'));
  for (const file of ['h9-toolbox-firmware-v0.1.6-rc.5.fw', 'h9-toolbox-v0.1.6-rc.5-far.bin']) {
    assert.ok((await stat(join(site, 'dist/firmware-update/dev', file))).size > 0);
  }
});

test('the dev site stages the matching v0.1.7 firmware and app catalog by default', async t => {
  const site = await mkdtemp(join(tmpdir(), 'h9-dev-'));
  t.after(() => rm(site, { recursive: true, force: true }));
  const release = `firmware-research/module-loader/releases/${quickChangeBuild.version}`;
  for (const source of ['dist', 'scripts', 'prototypes/kiss-tnc', 'prototypes/aprs-update',
    'firmware-research/module-loader/smartbeacon-release',
    'firmware-research/module-loader/combined-preview-release',
    'firmware-research/kiss-tnc/APRS-1.0.15-rx15-rxff.bin', release]) {
    await cp(new URL(source, root), join(site, source), { recursive: true });
  }
  await promisify(execFile)('bash', ['scripts/build-dev-site.sh'], {
    cwd: site, env: { ...process.env, CF_PAGES_COMMIT_SHA: 'test' },
  });
  const updater = await readFile(join(site, 'dist/firmware-update/index.html'), 'utf8');
  assert.match(updater, /id="optBuild" data-release-firmware data-next-firmware/);
  assert.match(updater, /Load H9 Toolbox v0\.1\.7/);
  const apps = await readFile(join(site, 'dist/apps/index.html'), 'utf8');
  assert.match(apps, /id="catalog" data-release-catalog data-next-catalog/);
  const catalog = JSON.parse(await readFile(join(site, 'dist/apps/catalog/0.1.7/catalog.json'), 'utf8'));
  assert.ok(catalog.menu.notes.includes(`Use matching H9 Toolbox Firmware v${quickChangeBuild.version}.`));
  const load = async file => new Uint8Array(await readFile(join(site, 'dist/firmware-update', file)));
  const [pack, far] = await Promise.all([load(quickChangeBuild.file), load(quickChangeBuild.farFile)]);
  assert.equal(await sha256(pack), quickChangeBuild.packageSha256);
  assert.equal(await sha256(decodePackage(pack)), quickChangeBuild.imageSha256);
  assert.equal(await sha256(far), quickChangeBuild.farSha256);
  assert.equal(far.length, FAR_END - FAR_START);
});
