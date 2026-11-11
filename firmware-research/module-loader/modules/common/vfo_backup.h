/* The user's own VFO A / B frequencies, kept on the resource chip while SAT TRACK's tracker has
 * saved something else in their place (vfo_persist.h).
 *
 * Sector 0x3FE000 (just below the GPS fix's, never written by stock; the game slot stops before
 * it) is an append-only log of 32-byte records, the newest valid one wins, like fix_store.h:
 *   u32 "VFB2", u32 VFO A, u32 VFO B (10 Hz units), u16 TX tone A, u16 TX tone B (the record's
 *   two BCD bytes), u16 state, u16 check, then padding.
 * state 0 = nothing to restore; otherwise VFO_ARMED plus the parts the tracker wrote (frequency
 * of A / B, TX tone of A / B). The sector is erased only when all 128 records are used. */
#ifndef VFO_BACKUP_H
#define VFO_BACKUP_H

#include <stdint.h>

#include "fix_store.h"

#define VFO_BACKUP_SECTOR 0x3FE000u
enum { VFO_ARMED = 1, VFO_A_TOUCHED = 2, VFO_B_TOUCHED = 4, VFO_A_TONE = 8, VFO_B_TONE = 16 };

/* The newest record's state (0 if none), the saved frequencies and TX tones. */
unsigned vfo_backup_load(fix_read_fn read, uint32_t *a, uint32_t *b, uint16_t *tone_a, uint16_t *tone_b);
void vfo_backup_save(fix_read_fn read, uint32_t a, uint32_t b, uint16_t tone_a, uint16_t tone_b, unsigned state);

#endif
