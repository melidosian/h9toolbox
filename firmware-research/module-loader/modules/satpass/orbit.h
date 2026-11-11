/* Satellite pass prediction in integer arithmetic (orbit.c).
 *
 * Model: two-body Kepler orbit from TLE mean elements, with the secular J2
 * drift of RAAN, argument of perigee and mean anomaly, and the TLE's ndot/2
 * decay term. Not SGP4: it fits the module's 8 KiB slot and needs no floating
 * point or 64-bit division (the radio's toolchain calls a library routine for
 * those). Fine for pass times of low orbits with fresh elements; checked
 * against SGP4 by test_satpass.py.
 *
 * Units: angles are 32-bit binary angles (2^32 = one turn); sin/cos are Q30;
 * distances are km in Q8; time is Unix seconds (UTC). */
#ifndef SATPASS_ORBIT_H
#define SATPASS_ORBIT_H

#include <stdint.h>

/* One satellite, made by satpack.py from a TLE. */
struct sat {
    char name[8];             /* short label, NUL-padded */
    uint32_t m0, argp0, raan0;/* binary angles at the shared epoch (gmst_ref.unix) */
    int32_t cos_i, sin_i;     /* inclination, Q30 */
    int32_t ecc;              /* eccentricity, Q30 */
    int32_t a;                /* semi-major axis, km Q8 */
    int32_t argp_dot, raan_dot; /* binary angle per second, Q16 */
    int32_t m_dot;            /* mean motion with J2, binary angle per second, Q8 */
    int32_t m_ddot;           /* ndot/2 term, binary angle per second^2, Q40 */
};

/* Shared epoch of the table (satpack.py) and GMST at it. */
/* A satellite's frequencies (satdata.h): the downlink it sends and the uplink it hears, in 10 Hz units,
 * and the access tone in tenths of a hertz (0 = none). */
struct satfreq {
    uint32_t down10, up10;
    uint16_t tone10, spare;
};

struct gmst_ref {
    uint32_t unix;
    uint32_t angle;
};

struct observer {
    int32_t x, y, z;                      /* ECEF, km Q8 */
    int32_t sin_lat, cos_lat, sin_lon, cos_lon;  /* Q30 */
};

struct look {
    int32_t el;   /* elevation, binary angle (signed) */
    uint32_t az;  /* azimuth, binary angle from north, clockwise */
    int32_t range; /* slant range, km Q8 */
};

struct pass {
    uint8_t sat;              /* index into the table */
    uint8_t max_el;           /* degrees */
    uint16_t az_aos, az_los;  /* degrees */
    uint32_t aos, los, tca;   /* Unix seconds (aos may be "now": already up) */
};

void orbit_sincos(uint32_t angle, int32_t *sin_q30, int32_t *cos_q30);
/* atan2(y, x) as a binary angle; *mag = hypot(x, y). */
uint32_t orbit_atan2(int32_t y, int32_t x, int32_t *mag);

/* Observer from latitude / longitude in binary angles (WGS84, sea level). */
void orbit_observer(struct observer *o, int32_t lat, int32_t lon);
void orbit_look(const struct sat *s, const struct gmst_ref *g, const struct observer *o, uint32_t t, struct look *out);

/* The next `want` passes over all `count` satellites (fetched one by one with `get`) from `now`, searching `hours`
 * ahead, sorted by AOS; returns how many were found. Passes whose maximum
 * elevation is below `min_el_deg` are skipped. */
typedef void (*orbit_get)(void *ctx, unsigned index, struct sat *out);   /* fetch satellite `index` */
unsigned orbit_passes(orbit_get get, void *ctx, unsigned count, const struct gmst_ref *g, const struct observer *o,
                      uint32_t now, unsigned hours, unsigned min_el_deg, struct pass *out, unsigned want);

/* Binary angle to whole degrees (0..359 or -180..179 for signed use). */
unsigned orbit_deg(uint32_t angle);

#endif
