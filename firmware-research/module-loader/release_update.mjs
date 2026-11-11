// Move the web installer (dist/firmware-update/) to a new module-loader build.
//
//   node release_update.mjs <new.fw> <dist/firmware-update> <id> <label>
//
// Rebuilds every image the installer knows from its hosted files (the hosted
// build, stock, and each PREVIOUS image, all checked by SHA-256), then writes
// seeds against the new build: each holds an image's bytes at the sectors
// where it differs from the new build. The currently hosted build becomes a
// PREVIOUS entry named <id> / <label>. Writes the new package under the same
// file name and prints the BUILD / PREVIOUS constants for firmware.mjs.
import { readFile, writeFile } from 'node:fs/promises';
import { join } from 'node:path';
import { APP_START, BUILD, IMAGE_SIZE, PREVIOUS, SECTOR_SIZE, STOCK_IMAGE_SHA256, decodePackage, sha256 }
  from '../../dist/firmware-update/firmware.mjs';

const [newPath, dir, id, label] = process.argv.slice(2);
if (!label) throw new Error('usage: node release_update.mjs <new.fw> <dist/firmware-update> <id> <label>');
const read = async path => new Uint8Array(await readFile(path));
const need = async (bytes, hash, what) => { if (await sha256(bytes) !== hash) throw new Error(`${what}: unexpected SHA-256`); return bytes; };
const patch = (base, seed, sectors) => {
  const image = base.slice();
  sectors.forEach((p, i) => image.set(seed.subarray(i * SECTOR_SIZE, (i + 1) * SECTOR_SIZE), p));
  return image;
};
const same = (a, b, p) => a.subarray(p, p + SECTOR_SIZE).every((v, i) => v === b[p + i]);
function seedFor(image, build) {
  const sectors = [];
  for (let p = APP_START; p < IMAGE_SIZE; p += SECTOR_SIZE) if (!same(image, build, p)) sectors.push(p);
  const seed = new Uint8Array(sectors.length * SECTOR_SIZE);
  sectors.forEach((p, i) => seed.set(image.subarray(p, p + SECTOR_SIZE), i * SECTOR_SIZE));
  return { sectors, seed };
}
const hex = list => `[${list.map(p => `0x${p.toString(16)}`).join(', ')}]`;

const hostedPackage = await need(await read(join(dir, BUILD.packageFile)), BUILD.packageSha256, 'hosted package');
const hosted = await need(decodePackage(hostedPackage), BUILD.imageSha256, 'hosted image');
const stock = await need(patch(hosted, await need(await read(join(dir, BUILD.stockSeedFile)), BUILD.stockSeedSha256, 'stock seed'),
  BUILD.sectors), STOCK_IMAGE_SHA256, 'stock image');
const previous = [];
for (const entry of PREVIOUS) {
  const seed = await need(await read(join(dir, entry.seedFile)), entry.seedSha256, `${entry.id} seed`);
  previous.push({ ...entry, image: await need(patch(hosted, seed, entry.sectors), entry.imageSha256, `${entry.id} image`) });
}
if (PREVIOUS.some(entry => entry.id === id)) throw new Error(`${id} is already a PREVIOUS entry`);
previous.push({ id, label, file: 'H9-Toolbox-Firmware-' + id + '-', seedFile: `h9-toolbox-firmware-${id}-sectors.bin`,
  imageSha256: BUILD.imageSha256, image: hosted });

const newPackage = await read(newPath);
const build = decodePackage(newPackage);
const stockSeed = seedFor(stock, build);
// Every sector any H9 Toolbox image differs from stock in (installs and restores write only these).
const all = new Set(stockSeed.sectors);
for (const entry of previous) for (const p of seedFor(entry.image, stock).sectors) all.add(p);
const sectors = [...all].sort((a, b) => a - b);
const stockOut = seedFor(stock, build);
// The stock seed must cover the full write set so a restore rebuilds stock exactly.
const stockFull = new Uint8Array(sectors.length * SECTOR_SIZE);
sectors.forEach((p, i) => stockFull.set(stock.subarray(p, p + SECTOR_SIZE), i * SECTOR_SIZE));
if (await sha256(patch(build, stockFull, sectors)) !== STOCK_IMAGE_SHA256) throw new Error('stock seed does not rebuild stock');
void stockOut;

await writeFile(join(dir, BUILD.packageFile), newPackage);
await writeFile(join(dir, BUILD.stockSeedFile), stockFull);
const lines = [];
for (const entry of previous) {
  const { sectors: s, seed } = seedFor(entry.image, build);
  if (await sha256(patch(build, seed, s)) !== entry.imageSha256) throw new Error(`${entry.id} seed does not rebuild it`);
  await writeFile(join(dir, entry.seedFile), seed);
  lines.push(`  { id: '${entry.id}', label: '${entry.label}', file: '${entry.file}',\n` +
    `    seedFile: '${entry.seedFile}',\n    seedSha256: '${await sha256(seed)}',\n` +
    `    imageSha256: '${entry.imageSha256}',\n    sectors: ${hex(s)} },`);
}
console.log(`BUILD.packageSha256: '${await sha256(newPackage)}'`);
console.log(`BUILD.imageSha256: '${await sha256(build)}'`);
console.log(`BUILD.stockSeedSha256: '${await sha256(stockFull)}'`);
console.log(`BUILD.sectors: ${hex(sectors)}`);
console.log('PREVIOUS = [\n' + lines.join('\n') + '\n];');
