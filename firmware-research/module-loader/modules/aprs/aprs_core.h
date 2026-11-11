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

/* Stock RAM the screens share. Each can be overridden (the host test does). */
#ifndef RING
#define RING ((volatile uint8_t *)0x16634)               /* APRS modem serial ring, 2048 B */
#define RING_WRITE (*(volatile uint16_t *)0x16E34)
#define RING_READ (*(volatile uint16_t *)0x16E36)
#endif
#define RING_MASK 2047u
/* Stock timer-3 counter: 100 Hz, measured on hardware with a stopwatch and
 * running while a module has the CPU (resource-flash-log.md, 2026-11-09). */
#ifndef TICK
#define TICK (*(const volatile uint32_t *)0x1D8B0)
#endif

enum { STATIONS = 16, CALL = 10, TEXT = 24, MESSAGES = 6, MSG_TEXT = 40, FRAME_MAX = 124, NMEA_MAX = 64 };
enum { TITLE_H = 18, BODY_Y = 20, FOOT_Y = 120 };
enum { EV_REDRAW = 1, EV_UP, EV_DOWN, EV_MENU, EV_HOLD, EV_BACK };
/* Digit key d (stock key code d + 2): EV_DIGIT + d, on the press. */
enum { EV_DIGIT = 16 };

struct station {
    int32_t lat, lon;          /* hundredths of a minute; north and east positive */
    char call[CALL];
    char text[TEXT];
    uint16_t heard;            /* when it was last heard live: units of 10 s of the stock tick (put_age) */
    uint8_t has_pos, table, symbol, live;    /* (fields ordered so the struct has no padding) */
};

struct message {
    char from[CALL], to[CALL];
    char text[MSG_TEXT];
    uint8_t mine : 1, live : 1, acked : 1;   /* to me; heard live; (sent by me) acknowledged by the other station */
    uint8_t id;                              /* the {id of a message I sent: its ack carries it */
};

struct aprs {
    uint32_t magic;
    struct station st[STATIONS];
    struct message msg[MESSAGES];
    uint32_t rx, rx_flash, press_tick;
    int32_t my_lat, my_lon;
    uint16_t length, held;
    uint8_t count, msgs, unread, my_fix, gps_seen, older, switching, escape, nmea_len;
    uint8_t list_sel, list_top, map_zoom, map_sel, msg_sel, msg_top;   /* screen state */
    uint8_t send_sel, msg_id;                                          /* APRS SEND */
    uint8_t dirty, last_key, home_side, home_speaker;                  /* before APRS */
    uint8_t beacon;                                                    /* APRS BEACON's settings, kept across screens (BEACON_* below) */
    char mycall[CALL];
    uint8_t frame[FRAME_MAX];
    char nmea[NMEA_MAX];
};

#define APRS_BYTES ((sizeof(struct aprs) + 15u) & ~15u)
#define APRS_SLOT_BYTES 0x570u   /* the state (sizeof, rounded up to 16) plus a little; build.py's APRS code limit follows it */
#define APRS_ADDR (H9_SHARED_TOP - APRS_SLOT_BYTES)
#ifdef APRS_HOST_TEST
extern struct aprs aprs_host;
#define A (&aprs_host)
#else
#define A ((struct aprs *)APRS_ADDR)
#endif
enum { APRS_MAGIC = 0x33525041 };  /* "APR3": layout of 2026-11-11 (0x570 slot) */

/* APRS BEACON settings byte: bit 0 mode (0 smart, 1 timed), bits 1-2 symbol (0 person, 1 bike, 2 car),
 * bits 3-5 the timed interval's index (aprs_beacon.c's table). A fresh state starts smart, person, 5 min. */
enum { BEACON_DEFAULT = 2 << 3 };

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
/* How long ago a live station was heard: "NOW", "5M", "2H" (nothing for stored ones). */
unsigned put_age(char *out, const struct station *s);
/* The five screens, for the tab bar (1 LIST 2 MAP 3 MSG 4 SEND 5 BCN). aprs_tabs draws the bar on the
 * bottom line (`cur` is kept for a highlight); aprs_tab handles a digit key: if it names another
 * screen that is installed it asks the loader for it and returns 1 (module_main then returns 0). */
enum { TAB_LIST, TAB_MAP, TAB_MSG, TAB_SEND, TAB_BEACON, TAB_NONE };
void aprs_tabs(unsigned cur);
int aprs_tab(const struct h9_api *api, unsigned ev, unsigned cur);
unsigned put_uint(char *out, unsigned v);
unsigned put_str(char *out, const char *s);
void aprs_title(const char *text);
void aprs_line(unsigned y, const char *text, uint16_t fg, uint16_t bg);
/* Draw `text` wrapped at 26 columns from y while y < limit; returns next y. */
unsigned aprs_wrap(unsigned y, const char *text, unsigned limit, uint16_t fg);

/* exposed for the host test */
void aprs_feed(uint8_t b);
void aprs_reset(void);

/* "APRS" (the menu entry, aprs_load.c) loads stock's Beacon List, then runs the station list. */
#define APRS_LIST ((const char *)".APRS LIST")
#define APRS_INFO ((const char *)".APRS INFO")
#define APRS_MAP ((const char *)"APRS MAP")
#define APRS_MSG ((const char *)"APRS MSG")
#define APRS_SEND ((const char *)"APRS SEND")
#define APRS_BEACON ((const char *)"APRS BEACON")

#endif
