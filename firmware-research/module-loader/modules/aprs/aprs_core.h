/* APRS client core, linked into each APRS screen module (APRS, APRS MAP,
 * APRS MSG). The screens are separate modules that the loader swaps in and
 * out of the 8 KiB RAM slot; what they share lives in `struct aprs` at a
 * fixed address just below the request mailbox, which module code never
 * reaches, so it survives a switch.
 *
 * Data sources (firmware-research/resource-flash-log.md, 2026-11-10):
 *   - APRS modem serial ring: 0x16634 (2048 B), write h[0x16E34], read h[0x16E36];
 *     frames C0 <cmd> <payload> C0, cmd 0x00 = AX.25 frame, 0x10 = GPS NMEA.
 *   - Stock Beacon List: chip 0x80B600 (read only).
 *   - Own callsign: RAM 0xC4A1 (6 chars), SSID byte 0xC4A7 (stock's receive
 *     handler compares against these). */
#ifndef APRS_CORE_H
#define APRS_CORE_H

#include "h9mod.h"
#include "gfx.h"

enum { STATIONS = 16, CALL = 10, TEXT = 24, MESSAGES = 6, MSG_TEXT = 40, FRAME_MAX = 200, NMEA_MAX = 72 };
enum { TITLE_H = 18, BODY_Y = 20, FOOT_Y = 120 };
enum { EV_REDRAW = 1, EV_UP, EV_DOWN, EV_MENU, EV_HOLD, EV_BACK };

struct station {
    char call[CALL];
    char text[TEXT];
    int32_t lat, lon;          /* hundredths of a minute; north and east positive */
    uint8_t has_pos, table, symbol, live;
};

struct message {
    char from[CALL], to[CALL];
    char text[MSG_TEXT];
    uint8_t mine, live;
};

struct aprs {
    uint32_t magic;
    struct station st[STATIONS];
    struct message msg[MESSAGES];
    unsigned count, msgs, unread, rx;
    int32_t my_lat, my_lon;
    uint8_t my_fix, gps_seen, older, switching;
    char mycall[CALL];
    uint8_t frame[FRAME_MAX];
    unsigned length, escape, nmea_len;
    char nmea[NMEA_MAX];
    unsigned rx_flash, last_key, held, press_tick, dirty;
    unsigned list_sel, list_top, map_zoom, map_sel, msg_sel, msg_top;   /* screen state */
    unsigned send_sel, msg_id;                                           /* APRS SEND */
    unsigned home_side, home_speaker;                                    /* before APRS */
};

#define APRS_BYTES ((sizeof(struct aprs) + 15u) & ~15u)
#define APRS_SLOT_BYTES 0x600u
#define APRS_ADDR (H9_SHARED_TOP - APRS_SLOT_BYTES)
#ifdef APRS_HOST_TEST
extern struct aprs aprs_host;
#define A (&aprs_host)
#else
#define A ((struct aprs *)APRS_ADDR)
#endif
enum { APRS_MAGIC = 0x32525041 };  /* "APR2": layout of 2026-11-10 (0x600 slot) */

/* Open the shared state: keep it across a screen switch. Returns 1 on a
 * fresh start (from the menu): partial frames dropped, own callsign reread;
 * the station list then also reloads stock's Beacon List. */
int aprs_open(void);
/* Stock Beacon List into the station table (aprs_stored.c; station list only). */
void aprs_load_stored(const struct h9_api *api);
/* Radio side 0 = A, 1 = B: put the radio there as stock's APRS beacon sender
 * does (logical side 0x11B50 bit 7, Dual Watch route 0x11AEC, then stock RF
 * setup 0x01E58E3E). aprs_rx_side/aprs_tx_side: the side the APRS RX CH /
 * TX CH settings choose (2 = keep the current side). */
void aprs_side(unsigned side);
/* Receive audio on (as with squelch open) and the built-in speaker off: the
 * APRS chip hears everything while the app runs, quietly. */
void aprs_quiet(void);
#define SPEAKER_ON(on) ((void (*)(unsigned))0x01E52328u)(on)
unsigned aprs_rx_side(void);
unsigned aprs_tx_side(void);
/* Poll the ring and keys until something happens; returns EV_*.
 * Menu: EV_MENU on a short press, EV_HOLD once when held. */
unsigned aprs_event(void);
/* If module `name` is installed, ask the loader to run it next and return 1
 * (module_main then returns 0); otherwise return 0. */
int aprs_go(const struct h9_api *api, const char *name);
/* One decoded packet into the tables (used by aprs_stored.c). */
void aprs_packet(const char *dest, const char *from, const uint8_t *info, unsigned len, int live);
void aprs_call_of(const uint8_t *address, int raw, char *out);

/* Offset of (lat, lon) from own position in hundredths of a minute, east
 * scaled by cos(latitude): both axes then measure 18.52 m per unit. */
void aprs_offset(int32_t lat, int32_t lon, int32_t *east, int32_t *north);
int aprs_range(const struct station *s, unsigned *tenths_km, const char **point);
unsigned put_range(char *out, const struct station *s);
unsigned put_uint(char *out, unsigned v);
unsigned put_str(char *out, const char *s);
void aprs_title(const char *text);
void aprs_line(unsigned y, const char *text, uint16_t fg, uint16_t bg);
/* Draw `text` wrapped at 26 columns from y while y < limit; returns next y. */
unsigned aprs_wrap(unsigned y, const char *text, unsigned limit, uint16_t fg);

/* exposed for the host test */
void aprs_feed(uint8_t b);
void aprs_reset(void);

#define APRS_LIST ((const char *)"APRS")
#define APRS_INFO ((const char *)".APRS INFO")
#define APRS_MAP ((const char *)"APRS MAP")
#define APRS_MSG ((const char *)"APRS MSG")
#define APRS_SEND ((const char *)"APRS SEND")

#endif
