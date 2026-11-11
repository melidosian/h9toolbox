/* Last GPS fix kept on the resource chip across power-off (fix_store.c).
 *
 * Sector 0x3FF000 (the last of the voice partition's erased tail, never
 * written by stock; the game slot stops before it) holds an append-only log
 * of 16-byte records; the newest valid one wins. The sector is erased only
 * when all 256 records are used. Record: u32 magic "GFX1", i32 lat, i32 lon
 * (signed microdegrees, as stock's GPS parser keeps them), u16 day (0 for
 * now), u16 check (see check_of). The loader stub's background recorder
 * (src/stub.s rec_tick) is the only writer; modules only read. fix_load
 * returns microdegrees; fix_bam converts to binary angles (2^32 = one turn). */
#ifndef FIX_STORE_H
#define FIX_STORE_H

#include <stdint.h>

#define FIX_STORE_SECTOR 0x3FF000u

typedef void (*fix_read_fn)(void *dst, uint32_t chip_address, uint32_t length);

/* 1 and the fix (signed microdegrees) if one is stored, else 0. */
int fix_load_micro(fix_read_fn read, int32_t *lat, int32_t *lon);

/* microdegrees -> binary angle (2^32 per turn): x 2^32 / 360e6 */
static inline int32_t fix_bam(int32_t micro) { return (int32_t)(((int64_t)micro * 200159983) >> 24); }

#endif
