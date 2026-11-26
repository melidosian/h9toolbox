import test from 'node:test';
import assert from 'node:assert/strict';
import { execFile } from 'node:child_process';
import { cp, mkdir, mkdtemp, readFile, rm, stat } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { promisify } from 'node:util';
import { BUILD, decodePackage, sha256 } from '../dist/firmware-update/firmware.mjs';
import { availableCombinedBuilds, selectCombinedBuild } from '../dist/firmware-update/combined-builds.mjs';

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
    assert.equal(BUILD.version, text);
  }
  assert.ok(BUILD.label.endsWith(`v${BUILD.version}`), 'the label names the published version');
  assert.equal(BUILD.packageFile, `h9-toolbox-firmware-v${BUILD.version}.fw`);
});

test('the GitHub Pages build publishes v0.1.7 as the default firmware with its app catalog', async t => {
  const workflow = await readFile(new URL('.github/workflows/pages.yml', root), 'utf8');
  assert.match(workflow, /run: bash scripts\/build-site\.sh/);
  const site = await mkdtemp(join(tmpdir(), 'h9-pages-'));
  t.after(() => rm(site, { recursive: true, force: true }));
  for (const dir of ['dist', 'scripts', 'prototypes/kiss-tnc', 'prototypes/aprs-update',
    'firmware-research/h9-flasher',
    'firmware-research/kiss-tnc', 'firmware-research/module-loader/releases',
    'firmware-research/module-loader/smartbeacon-release',
    'firmware-research/module-loader/combined-preview-release']) {
    await mkdir(join(site, dir), { recursive: true });
    await cp(new URL(`${dir}/`, root), join(site, dir), { recursive: true });
  }
  await promisify(execFile)('bash', ['scripts/build-site.sh'], { cwd: site });
  const updater = await readFile(join(site, 'dist/firmware-update/index.html'), 'utf8');
  assert.match(updater, /id="optBuild" data-release-firmware data-next-firmware/);
  assert.match(updater, /Load H9 Toolbox v0\.1\.7/);
  assert.doesNotMatch(updater, /data-legacy-dev-firmware|Load H9 Toolbox v0\.1\.5/);
  assert.match(updater, /href="\.\/h9-desktop-backup\.zip"/);
  assert.match(updater, /<input id="exactBackup" type="checkbox" disabled>/);
  assert.match(updater, /<input type="checkbox" id="includeModem" checked>/);
  assert.match(updater, /id="flashMap"/);
  assert.doesNotMatch(updater, /data-33cm-firmware/);
  assert.doesNotMatch(updater, /id="skipBackup"/);
  const zip = join(site, 'dist/firmware-update/h9-desktop-backup.zip');
  await promisify(execFile)('python3', ['-m', 'zipfile', '-e', zip, site]);
  const { stdout } = await promisify(execFile)('python3', [
    join(site, 'h9-desktop-backup/h9_flasher.py'), 'recovery-backup', '--help',
  ]);
  assert.match(stdout, /--device.*--output/s);
  const apps = await readFile(join(site, 'dist/apps/index.html'), 'utf8');
  assert.match(apps, /id="catalog" data-release-catalog data-next-catalog/);
  assert.doesNotMatch(apps, /data-legacy-dev-catalog/);
  const catalog = JSON.parse(await readFile(join(site, 'dist/apps/catalog/rc5/catalog.json'), 'utf8'));
  assert.ok(catalog.apps.some(app => app.id === 'aprs'));
  for (const file of ['h9-toolbox-firmware-v0.1.6-rc.4.fw', 'h9-toolbox-v0.1.6-rc.4-far.bin',
    'h9-toolbox-firmware-v0.1.6-rc.5.fw', 'h9-toolbox-v0.1.6-rc.5-far.bin']) {
    assert.ok((await stat(join(site, 'dist/firmware-update/dev', file))).size > 0);
  }
  const farWithout = await readFile(join(site, 'dist/firmware-update/dev/h9-toolbox-v0.1.6-rc.4-far.bin'));
  const farWith = await readFile(join(site, 'dist/firmware-update/dev/h9-toolbox-v0.1.6-rc.5-far.bin'));
  const modemStart = 0xf7a00 - 0xea000, modemEnd = 0xfa000 - 0xea000;
  assert.equal(farWithout.length, farWith.length);
  assert.deepEqual(farWithout.subarray(0, modemStart), farWith.subarray(0, modemStart));
  assert.ok(farWithout.subarray(modemStart, modemEnd).every(byte => byte === 0xff));
  assert.ok(farWith.subarray(modemStart, modemEnd).some(byte => byte !== 0xff));
  assert.deepEqual(farWithout.subarray(modemEnd), farWith.subarray(modemEnd));

  async function verifyBuilds(builds) {
    for (const build of builds) {
      const pkg = new Uint8Array(await readFile(join(site, 'dist/firmware-update', build.file)));
      const far = await readFile(join(site, 'dist/firmware-update', build.farFile));
      assert.equal(await sha256(pkg), build.packageSha256);
      assert.equal(await sha256(decodePackage(pkg)), build.imageSha256);
      assert.equal(far.length, 0x12000);
      assert.equal(await sha256(far), build.farSha256);
      const catalogDir = build.catalog === '0.1.6' ? 'rc5' : build.catalog;
      const matching = JSON.parse(await readFile(join(site, 'dist/apps/catalog', catalogDir, 'catalog.json'), 'utf8'));
      assert.ok(matching.apps.some(app => app.id === 'aprs'));
      assert.match(matching.menu.notes.join(' '), new RegExp(build.version.replaceAll('.', '\\.')));
      for (const item of [matching.menu, ...matching.apps])
        assert.equal(await sha256(await readFile(join(site, 'dist/apps/catalog', catalogDir, item.file))), item.sha256);
      const about = matching.apps.find(app => app.id === 'about');
      assert.equal(about.version, '1.1');
      assert.equal(await sha256(await readFile(join(site, 'dist/apps/catalog', catalogDir, about.file))), about.sha256);
    }
  }
  await verifyBuilds(availableCombinedBuilds(false));
  await assert.rejects(stat(join(site, 'dist/apps/catalog/0.1.8')));
  await assert.rejects(stat(join(site, 'dist/firmware-update/dev/h9-toolbox-firmware-v0.1.8.fw')));
  await promisify(execFile)('bash', ['scripts/build-dev-site.sh'], {
    cwd: site, env: { ...process.env, CF_PAGES_COMMIT_SHA: 'test' },
  });
  assert.match(await readFile(join(site, 'dist/firmware-update/index.html'), 'utf8'), /data-33cm-firmware/);
  assert.match(await readFile(join(site, 'dist/apps/index.html'), 'utf8'), /data-33cm-catalog/);
  assert.match(await readFile(join(site, 'dist/firmware-update/index.html'), 'utf8'), /Load H9 Toolbox v0\.1\.8/);
  assert.match(await readFile(join(site, 'dist/firmware/index.html'), 'utf8'), /V0\.1\.8/);
  await verifyBuilds(availableCombinedBuilds(true).filter(build => build.version === '0.1.8'));
});

test('installer options resolve separately from the modem updater and gate 33 cm to Dev', () => {
  assert.equal(selectCombinedBuild().id, '0.1.6-rc.5');
  assert.equal(selectCombinedBuild({ quickControls: true }).id, '0.1.7');
  assert.equal(selectCombinedBuild({ quickControls: true, includeModem: false }).id, '0.1.7-no-modem');
  assert.equal(selectCombinedBuild({ quickControls: true, band33cm: true, dev: true, includeModem: false }).id, '0.1.8-no-modem');
  assert.throws(() => selectCombinedBuild({ quickControls: true, band33cm: true }), /development installer/);
  assert.throws(() => selectCombinedBuild({ band33cm: true, dev: true }), /Quick Change/);
});
