// Make the web installer's hosted files for the module-loader build of
// H9 Toolbox Firmware (dist/firmware-update/), and print the constants for
// firmware.mjs. Reproducible: everything is checked by SHA-256.
//
//   node release_installer.mjs <new.fw> <preview.fw> <dist/firmware-update>
//
// Inputs read from the installer directory before it is overwritten: the
// currently hosted package (the Brick Breaker build of H9 Toolbox Firmware),
// its stock-sector seed, and the original Brick Breaker's seed. Outputs:
//   h9-toolbox-firmware-v1.0.38.fw           the new package (same name: the
//                                            Doom installer fetches it too)
//   h9-toolbox-firmware-stock-sectors.bin    stock bytes where the new build differs
//   h9-toolbox-firmware-1-sectors.bin        the Brick Breaker build of H9 Toolbox
//   brick-breaker-original-sectors.bin       the original Brick Breaker
//   h9-toolbox-firmware-preview-sectors.bin  the module-loader preview (79858310…)
// Each seed holds an image's bytes at the sectors where it differs from the
// new build, so the installer rebuilds and recognises it exactly.
import { readFile, writeFile } from 'node:fs/promises';
import { join } from 'node:path';
import { APP_START, IMAGE_SIZE, SECTOR_SIZE, decodePackage, sha256 } from '../../dist/firmware-update/firmware.mjs';

const OLD = {
  packageSha256: 'b9d24cb43937621358053356382ab72608bfbab677468febe4db867242098dc5',
  imageSha256: '83f79fb3281c37ab22a4fd3f154a6c52a50d3cf0d8063b546eb47ce3c603bac6',
  stockSeedSha256: 'ae543e828cc0ad08326c04586d353b83fd44fa975ab72da34297967d631facea',
  sectors: [0x5000, 0x62000, 0x8d000, 0xaa000, 0xac000, 0xae000, 0xaf000, 0xd9000],
  originalSeedSha256: '1cc3367bfa4760a02b04897afa0e863e4e8a260a220c2883e63a9536de77f30a',
  originalImageSha256: '741bbeb87eb84cd7c6988054c56a267bb73c89bbe5cc74dc4e5bc0d75ce7e193',
  originalSectors: [0x5000, 0x62000, 0xaa000, 0xac000, 0xae000, 0xaf000, 0xd9000],
};
const STOCK_IMAGE_SHA256 = '74243eaed19fe0c3e5fbb3ef3c46442f71ee8f7c216f214d322894d26ce4e648';
const PREVIEW_IMAGE_SHA256 = '79858310c73879a360e3f8c0cfb003a0043d74593a05421ddeb6f81b728bbcc1';

const [newPath, previewPath, dir] = process.argv.slice(2);
if (!dir) throw new Error('usage: node release_installer.mjs <new.fw> <preview.fw> <dist/firmware-update>');
const read = async path => new Uint8Array(await readFile(path));
const need = async (bytes, hash, what) => { if (await sha256(bytes) !== hash) throw new Error(`${what}: unexpected SHA-256`); return bytes; };
const sectorsOf = () => { const list = []; for (let p = APP_START; p < IMAGE_SIZE; p += SECTOR_SIZE) list.push(p); return list; };
const same = (a, b, p) => a.subarray(p, p + SECTOR_SIZE).every((v, i) => v === b[p + i]);
function patch(base, seed, sectors) {
  const image = base.slice();
  sectors.forEach((p, i) => image.set(seed.subarray(i * SECTOR_SIZE, (i + 1) * SECTOR_SIZE), p));
  return image;
}
function seedFor(image, build) {
  const sectors = sectorsOf().filter(p => !same(image, build, p));
  const seed = new Uint8Array(sectors.length * SECTOR_SIZE);
  sectors.forEach((p, i) => seed.set(image.subarray(p, p + SECTOR_SIZE), i * SECTOR_SIZE));
  return { sectors, seed };
}

const oldPackage = await need(await read(join(dir, 'h9-toolbox-firmware-v1.0.38.fw')), OLD.packageSha256, 'hosted package');
const oldImage = await need(decodePackage(oldPackage), OLD.imageSha256, 'hosted image');
const stock = await need(patch(oldImage, await need(await read(join(dir, 'h9-toolbox-firmware-stock-sectors.bin')), OLD.stockSeedSha256, 'stock seed'), OLD.sectors), STOCK_IMAGE_SHA256, 'stock image');
const original = await need(patch(oldImage, await need(await read(join(dir, 'brick-breaker-original-sectors.bin')), OLD.originalSeedSha256, 'original seed'), OLD.originalSectors), OLD.originalImageSha256, 'original image');
const preview = await need(decodePackage(await read(previewPath)), PREVIEW_IMAGE_SHA256, 'preview image');
const newPackage = await read(newPath);
const build = decodePackage(newPackage);
for (let p = 0; p < APP_START; p++) if (build[p] !== stock[p]) throw new Error('boot region differs from stock');

const images = { stock, old: oldImage, original, preview };
const out = {};
for (const [key, image] of Object.entries(images)) out[key] = seedFor(image, build);
// Every sector any H9 Toolbox image (or stock) differs in: the pinned write set.
const pinned = [...new Set(Object.values(out).flatMap(o => o.sectors))].sort((a, b) => a - b);

const files = { stock: 'h9-toolbox-firmware-stock-sectors.bin', old: 'h9-toolbox-firmware-1-sectors.bin',
  original: 'brick-breaker-original-sectors.bin', preview: 'h9-toolbox-firmware-preview-sectors.bin' };
await writeFile(join(dir, 'h9-toolbox-firmware-v1.0.38.fw'), newPackage);
for (const [key, name] of Object.entries(files)) await writeFile(join(dir, name), out[key].seed);

const hex = list => `[${list.map(p => `0x${p.toString(16)}`).join(', ')}]`;
console.log(JSON.stringify({
  packageSha256: await sha256(newPackage), imageSha256: await sha256(build), pinned: hex(pinned),
  seeds: Object.fromEntries(await Promise.all(Object.entries(files).map(async ([key, name]) =>
    [key, { file: name, sha256: await sha256(out[key].seed), sectors: hex(out[key].sectors), imageSha256: await sha256(images[key]) }]))),
}, null, 1));
