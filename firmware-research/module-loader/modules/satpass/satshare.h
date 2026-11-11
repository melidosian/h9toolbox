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
};

#define SAT_SHARED_BYTES 0x40u
#ifndef SAT_SHARED   /* host tests supply their own */
#define SAT_SHARED ((volatile struct satshare *)(0x2BEE0u - 0x600u - SAT_SHARED_BYTES))
#endif
#define SAT_MAGIC 0x544C5053u  /* "SPLT" */
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

/* The page tabs along the bottom of the SAT PASS screens: LIST and PLOT, the
 * current one in yellow. Drawn with gfx_text (include gfx.h first). */
static inline void sat_tabs(unsigned current)
{
    gfx_text(0, 119, "LIST PLOT", 9, 1, GFX_GREY, GFX_BLACK);
    gfx_text(current ? 30 : 0, 119, current ? "PLOT" : "LIST", 4, 1, GFX_YELLOW, GFX_BLACK);
}

#endif
