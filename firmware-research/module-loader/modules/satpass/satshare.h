/* State SAT PASS and its hidden pass-plot screen (.SAT PLOT) share across a
 * module switch. It sits just below APRS's shared state (aprs_core.h), where
 * the loader never writes: SAT PASS's code stays under SAT_SHARED, the plot's
 * too (build.py checks both). */
#ifndef SATSHARE_H
#define SATSHARE_H

#include <stdint.h>

struct satshare {
    uint32_t magic;            /* SAT_MAGIC: valid, written by SAT PASS when it opens the plot */
    int32_t lat, lon;          /* observer, binary angles */
    uint32_t now, tick;        /* UTC at the 100 Hz tick, so SAT PASS carries on from there */
    uint32_t aos[5];           /* the listed passes: rise (Unix seconds) */
    uint16_t dur_sat[5];       /* duration in seconds (bits 0-11) and table index (bits 12-15) */
    uint8_t sel, count;        /* the pass to show first, number of passes (1-5) */
    /* The resident VFO tracker (.SAT TRACK called from the loader stub's main-loop hook;
     * offsets 52..64 are read by src/stub.s res_tick). */
    uint32_t res_magic;        /* RES_MAGIC while the tracker is armed; the stub clears it when the menu opens */
    uint32_t res_entry;        /* the tracker's module_main in the RAM slot */
    uint32_t res_lencrc;       /* body length (high half) and CRC-16 (low half): the stub checks the slot once */
    uint32_t res_tick;         /* tick of the last call (the stub throttles to twice a second) */
    uint32_t st_ticks;         /* tracker status, left for VFO PROBE: calls so far */
    uint32_t st_down, st_up;   /* last frequencies written (10 Hz units; 0 = none yet) */
    uint32_t st_flags;         /* bit0 slot verified by the stub; bit1 transmit enabled; bit2 last call skipped (TX or no pass); bit3 test now; bit4 listen on VFO B and send on VFO A; bit5 open the squelch during the pass; bit6 the original squelch threshold is saved; bit7 the countdown is on the screen; bit8 the original PF1 short press is saved; bit9 make PF1 short press PTT2 while tracking */
    uint32_t st_valid;         /* RES_MAGIC once the tracker has been armed since power-up (RAM is random until then) */
    uint32_t st_reverts;       /* times stock put a VFO word back to another value after the tracker wrote it */
    uint16_t orig_sq;          /* the radio's own squelch threshold while the tracker holds it open (flag 64 = saved) */
    uint8_t orig_pf1, spare;   /* the radio's own PF1 short-press action while the tracker has it on PTT2 (flag 256 = saved) */
};

#define SAT_SHARED_BYTES 0x60u
#ifndef SAT_SHARED   /* host tests supply their own */
#define SAT_SHARED ((volatile struct satshare *)(0x2BEE0u - 0x600u - SAT_SHARED_BYTES))
#endif
#define SAT_MAGIC 0x544C5053u  /* "SPLT" */
#define RES_MAGIC 0x54525343u  /* "CSRT" little-endian bytes C S R T: armed */
#define RES_SELF 0xFFFFu       /* module_main(api, RES_SELF): the stub calls the tracker with this */
_Static_assert(sizeof(struct satshare) <= SAT_SHARED_BYTES, "satshare too big");

/* Stock's "Time zone" setting (UTC-12..+12, option index 0..24) as seconds.
 * The settings copy in RAM is the byte the stock NMEA code adds to the GPS
 * hour (CPU 0x01E86A76: [0xC0F0 + 0xD6] - 12); anything out of range = UTC. */
#ifndef H9_TZ_BYTE
#define H9_TZ_BYTE (*(const volatile uint8_t *)0xC1C6)
#endif
static inline uint32_t sat_local(uint32_t unix_time)
{
    unsigned v = H9_TZ_BYTE;
    return v <= 24 ? unix_time + (uint32_t)((int)v - 12) * 3600u : unix_time;
}
static inline char sat_zone_letter(void) { unsigned v = H9_TZ_BYTE; return v <= 24 && v != 12 ? 'L' : 'Z'; }

/* The page tabs along the bottom of the SAT PASS screens: LIST, PLOT and
 * TRACK, the current one in yellow. Drawn with gfx_text (include gfx.h first). */
static inline void sat_tabs(unsigned current)
{
    static const char TABS[] = "LIST PLOT TRACK";
    static const uint8_t AT[3] = { 0, 5, 10 }, LEN[3] = { 4, 4, 5 };
    gfx_text(0, 119, TABS, 15, 1, GFX_GREY, GFX_BLACK);
    gfx_text(AT[current] * 6u, 119, TABS + AT[current], LEN[current], 1, GFX_YELLOW, GFX_BLACK);
}

#endif
