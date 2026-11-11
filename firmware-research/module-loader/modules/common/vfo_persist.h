/* Saving a VFO frequency the way the stock stepper does (0x01E67B94, the branch at 0x01E67D16):
 * the radio keeps each VFO in the "rw_freq" settings partition (partition 4, A at 0x1950, B at
 * 0x1960, four bytes) and re-reads it whenever it refreshes the radio (PTT, squelch, side change),
 * overwriting the RAM words. A frequency that is only put in RAM is therefore lost at the next
 * refresh, and at PTT the transmit frequency is the saved one. 0x01E56038(freq, buf) encodes the
 * frequency (10 Hz units) into four bytes; 0x01E50128(4, offset, buf, 4) writes them (a sector
 * read-modify-write with interrupts off; stock does it on every dial step).
 *
 * The user's own frequencies are kept in vfo_backup.h and put back by vfo_restore_pending. */
#ifndef VFO_PERSIST_H
#define VFO_PERSIST_H

#include "vfo_backup.h"

#ifndef STOCK_ENCODE   /* the host test supplies its own */
#define STOCK_ENCODE(f, buf) ((void (*)(uint32_t, void *))0x01E56038)(f, buf)
#define STOCK_PARTITION_WRITE(part, offset, src, len) ((int (*)(unsigned, unsigned, const void *, unsigned))0x01E50128)(part, offset, src, len)
#define VFO_SCRATCH ((void *)(0x11AD0u + 0x574u))   /* the buffer the stepper uses */
#define STOCK_PARTITION_READ(part, offset, dst, len) ((int (*)(unsigned, unsigned, void *, unsigned))0x01E52CB2)(part, offset, dst, len)
#endif

static inline void vfo_save(unsigned side, uint32_t f10)
{
    STOCK_ENCODE(f10, VFO_SCRATCH);
    STOCK_PARTITION_WRITE(4, side ? 0x1960u : 0x1950u, VFO_SCRATCH, 4);
}

/* The record is the channel layout (dist/channel.mjs): bytes 8-9 the receive tone, 10-11 the transmit
 * tone (two BCD bytes, tenths of a hertz, low byte first: 67.0 Hz = 0x0670; 0xFFFF = none). The working
 * copies stock keeps are the halfwords at 0x11AD0 + 0x98 (receive) and + 0x9A (transmit: 0x01E5AD68
 * passes it to the tune); the record is what a refresh loads again. */
static inline void vfo_save_tone(unsigned side, uint16_t tone)
{
    uint8_t *b = (uint8_t *)VFO_SCRATCH;
    b[0] = (uint8_t)tone; b[1] = (uint8_t)(tone >> 8);
    STOCK_PARTITION_WRITE(4, (side ? 0x1960u : 0x1950u) + 10, VFO_SCRATCH, 2);
}
static inline uint16_t vfo_read_tone(unsigned side)
{
    uint8_t b[2] = { 0xFF, 0xFF };
    STOCK_PARTITION_READ(4, (side ? 0x1960u : 0x1950u) + 10, b, 2);
    return (uint16_t)(b[0] | b[1] << 8);
}
/* Two BCD bytes for a tone of `tenths` tenths of a hertz (670 -> 0x0670; 0 -> none). */
static inline uint16_t vfo_tone_code(unsigned tenths)
{
    if (!tenths) return 0xFFFF;
    return (uint16_t)((tenths / 1000 % 10) << 12 | (tenths / 100 % 10) << 8 | (tenths / 10 % 10) << 4 | (tenths % 10));
}

/* Put the user's VFO frequencies and TX tones back if the tracker changed them (and note that it is done). */
static inline void vfo_restore_pending(fix_read_fn read)
{
    uint32_t a = 0, b = 0;
    uint16_t ta = 0xFFFF, tb = 0xFFFF;
    unsigned state = vfo_backup_load(read, &a, &b, &ta, &tb);
    if (!(state & VFO_ARMED)) return;
    if (state & VFO_A_TOUCHED) vfo_save(0, a);
    if (state & VFO_B_TOUCHED) vfo_save(1, b);
    if (state & VFO_A_TONE) vfo_save_tone(0, ta);
    if (state & VFO_B_TONE) vfo_save_tone(1, tb);
    vfo_backup_save(read, a, b, ta, tb, 0);
}

#endif
