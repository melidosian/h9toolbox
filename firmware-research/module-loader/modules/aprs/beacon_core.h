/* APRS BEACON's hardware-free logic: the GPS sentences from the APRS modem's
 * KISS ring, the position packet, and (in aprs_beacon.c) the screen. Tested on
 * the host by test_beacon.c. */
#ifndef BEACON_CORE_H
#define BEACON_CORE_H
#include <stdint.h>

enum { BEACON_NMEA = 84, BEACON_INFO = 68 };

struct beacon_gps {
    char lat[9], lon[10];     /* "4903.50N" and "07201.75W": as the RMC gave them */
    uint16_t knots10;         /* speed over ground, tenths of a knot */
    uint16_t kmh10;           /* the same in tenths of a km/h */
    uint16_t course;          /* degrees; the last known one when the receiver gives none */
    uint8_t fix;              /* the last RMC had status A and a usable position */
    uint8_t seen;             /* any RMC seen at all */
};

struct beacon_feed {
    uint8_t started, cmd, esc;    /* inside a frame; its command byte; the escape flag */
    uint16_t len;                 /* NMEA line length */
    char nmea[BEACON_NMEA];
};

/* A position without a receiver fix: stock's saved last fix (signed microdegrees). Fills lat/lon (as "4152.68N" /
 * "08737.80W") and leaves speed at 0; g->fix stays 0. Returns 0 (g untouched) if a coordinate is out of range. */
int beacon_from_micro(struct beacon_gps *g, int32_t lat_micro, int32_t lon_micro);
/* Timed beaconing: the first position goes out 15 s after starting (`sent` is 0), then every `minutes`. */
int beacon_timed_due(uint32_t now_ms, uint32_t last_ms, unsigned sent, unsigned minutes);

/* One NUL-terminated NMEA sentence ("$GNRMC,..."); it is edited in place.
 * Returns 1 if it was an RMC (g updated, g->fix says whether it is usable). */
int beacon_rmc(char *line, struct beacon_gps *g);
/* One byte of the modem's KISS stream (C0 <cmd> <payload> C0; cmd 0x10 = GPS NMEA).
 * Returns 1 when an RMC sentence has just updated g. */
int beacon_feed(struct beacon_feed *f, uint8_t byte, struct beacon_gps *g);
/* The position report: "!4903.50N/07201.75W[090/005 comment" (course/speed are
 * left out below 1 knot). Returns its length. */
unsigned beacon_info(char *out, const struct beacon_gps *g, char symbol, const char *comment);
#endif
