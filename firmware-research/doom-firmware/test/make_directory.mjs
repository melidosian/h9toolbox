// Writes the directory sector the uploader would write for a pack of the given
// length, marked complete: the fixture for test_pack_find.c.
import { writeFileSync } from 'node:fs';
import { buildDirectory, completeBlock, COMPLETE_OFFSET } from '../../../prototypes/data-upload/upload.mjs';
const [length, out] = [Number(process.argv[2]), process.argv[3]];
const pack = new Uint8Array(length).fill(7);
const { sector } = await buildDirectory(pack, 'freedoom E1M1S');
sector.set(completeBlock(), COMPLETE_OFFSET);
writeFileSync(out, sector);
