/* Satellite pass prediction in integer arithmetic. See orbit.h. */
#include "orbit.h"

/* atan(2^-i) as binary angles, i = 0..23 (CORDIC; 24 steps resolve about
 * 6e-8 rad, under a metre at these orbits). */
enum { STEPS = 24 };
static const uint32_t ATAN[STEPS] = {
    536870912u, 316933406u, 167458907u, 85004756u, 42667331u, 21354465u, 10679838u, 5340245u, 2670163u,
    1335087u, 667544u, 333772u, 166886u, 83443u, 41722u, 20861u, 10430u, 5215u, 2608u, 1304u, 652u, 326u,
    163u, 81u,
};
#define CORDIC_K 652032874   /* 1/gain, Q30 (the same to 9 digits for 24 steps) */
#define RAD_TO_BAM 683565276 /* 2^32 / (2 pi) / 4, Q30: radians Q30 -> binary angle */
#define HALF 0x80000000u
#define QUARTER 0x40000000u
#define GMST_RATE 3266731825u /* 1.00273790935 turns per day, binary angle per second, Q16 */
#define EARTH_A (6378137 * 256 / 1000)        /* WGS84 equatorial radius, km Q8 */
#define EARTH_E2 7225969                       /* first eccentricity squared, Q30 */

static int32_t mul30(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 30); }

void orbit_sincos(uint32_t angle, int32_t *sin_q30, int32_t *cos_q30)
{
    int flip = 0;
    if (angle - QUARTER < HALF) {   /* (90, 270] degrees: rotate by 180 */
        angle += HALF;
        flip = 1;
    }
    int32_t x = CORDIC_K, y = 0, z = (int32_t)angle;
    for (unsigned i = 0; i < STEPS; i++) {
        int32_t dx = y >> i, dy = x >> i;
        if (z >= 0) { x -= dx; y += dy; z -= (int32_t)ATAN[i]; }
        else { x += dx; y -= dy; z += (int32_t)ATAN[i]; }
    }
    *sin_q30 = flip ? -y : y;
    *cos_q30 = flip ? -x : x;
}

uint32_t orbit_atan2(int32_t y, int32_t x, int32_t *mag)
{
    uint32_t angle = 0;
    if (x < 0) { x = -x; y = -y; angle = HALF; }
    int32_t z = 0;
    for (unsigned i = 0; i < STEPS; i++) {
        int32_t dx = y >> i, dy = x >> i;
        if (y > 0) { x += dx; y -= dy; z += (int32_t)ATAN[i]; }
        else { x -= dx; y += dy; z -= (int32_t)ATAN[i]; }
    }
    if (mag) *mag = mul30(x, CORDIC_K);
    return angle + (uint32_t)z;
}

unsigned orbit_deg(uint32_t angle)
{
    return (unsigned)(((uint64_t)angle * 360u + HALF) >> 32) % 360u;
}

void orbit_observer(struct observer *o, int32_t lat, int32_t lon)
{
    orbit_sincos((uint32_t)lat, &o->sin_lat, &o->cos_lat);
    orbit_sincos((uint32_t)lon, &o->sin_lon, &o->cos_lon);
    /* N = a / sqrt(1 - x), x = e2 sin^2 lat <= 0.0067: 1 + x/2 + 3x^2/8 is
     * within 1e-7 (under a metre) and needs no division. */
    int32_t x = mul30(EARTH_E2, mul30(o->sin_lat, o->sin_lat));
    int32_t inv = (1 << 30) + x / 2 + mul30(x, x) * 3 / 8;
    int32_t n = mul30(EARTH_A, inv);
    int32_t nc = mul30(n, o->cos_lat);
    o->x = mul30(nc, o->cos_lon);
    o->y = mul30(nc, o->sin_lon);
    o->z = mul30(mul30(n, (1 << 30) - EARTH_E2), o->sin_lat);
}

void orbit_look(const struct sat *s, const struct gmst_ref *g, const struct observer *o, uint32_t t, struct look *out)
{
    int32_t dt = (int32_t)(t - g->unix);
    int64_t d = dt;
    uint32_t m = s->m0 + (uint32_t)(((int64_t)s->m_dot * d) >> 8) + (uint32_t)(((((int64_t)s->m_ddot * d) >> 20) * d) >> 20);
    uint32_t w = s->argp0 + (uint32_t)(((int64_t)s->argp_dot * d) >> 16);
    uint32_t node = s->raan0 + (uint32_t)(((int64_t)s->raan_dot * d) >> 16);

    /* Kepler: E = M + e sin E (low eccentricity: a few iterations converge). */
    uint32_t e_anom = m;
    int32_t sin_e, cos_e;
    for (unsigned i = 0; i < 5; i++) {
        orbit_sincos(e_anom, &sin_e, &cos_e);
        e_anom = m + (uint32_t)mul30(mul30(s->ecc, sin_e), RAD_TO_BAM);
    }
    orbit_sincos(e_anom, &sin_e, &cos_e);
    /* b = a sqrt(1 - e^2) ~ a (1 - e^2 / 2): e < 0.02 here, error under 1e-7 */
    int32_t px = mul30(s->a, cos_e - s->ecc), py = mul30(s->a - mul30(s->a, mul30(s->ecc, s->ecc) / 2), sin_e);

    int32_t sw, cw, sn, cn;
    orbit_sincos(w, &sw, &cw);
    orbit_sincos(node, &sn, &cn);
    /* Rotate by perigee within the plane, then tilt by inclination and turn
     * by the node: 10 multiplies. */
    int32_t u = mul30(px, cw) - mul30(py, sw);   /* towards the node */
    int32_t v = mul30(px, sw) + mul30(py, cw);   /* in-plane, 90 degrees on */
    int32_t vc = mul30(v, s->cos_i);
    int32_t X = mul30(u, cn) - mul30(vc, sn);
    int32_t Y = mul30(u, sn) + mul30(vc, cn);
    int32_t Z = mul30(v, s->sin_i);

    /* Inertial to Earth-fixed by Greenwich sidereal time. */
    uint32_t theta = g->angle + (uint32_t)(((uint64_t)GMST_RATE * (uint64_t)(int64_t)(int32_t)(t - g->unix)) >> 16);
    int32_t st, ct;
    orbit_sincos(theta, &st, &ct);
    int32_t ex = mul30(ct, X) + mul30(st, Y);
    int32_t ey = mul30(ct, Y) - mul30(st, X);

    /* Range vector in the observer's east / north / up frame. */
    int32_t rx = ex - o->x, ry = ey - o->y, rz = Z - o->z;
    int32_t east = mul30(-o->sin_lon, rx) + mul30(o->cos_lon, ry);
    int32_t north = mul30(-mul30(o->sin_lat, o->cos_lon), rx) - mul30(mul30(o->sin_lat, o->sin_lon), ry)
                    + mul30(o->cos_lat, rz);
    int32_t up = mul30(mul30(o->cos_lat, o->cos_lon), rx) + mul30(mul30(o->cos_lat, o->sin_lon), ry)
                 + mul30(o->sin_lat, rz);
    int32_t horizontal;
    out->az = orbit_atan2(east, north, &horizontal);
    out->el = (int32_t)orbit_atan2(up, horizontal, 0);
}

/* First second in (lo, hi] where the elevation sign differs from at lo. */
static uint32_t crossing(const struct sat *s, const struct gmst_ref *g, const struct observer *o,
                         uint32_t lo, uint32_t hi, int up_at_lo)
{
    struct look l;
    while (hi - lo > 1) {
        uint32_t mid = lo + (hi - lo) / 2;
        orbit_look(s, g, o, mid, &l);
        if ((l.el >= 0) == up_at_lo) lo = mid;
        else hi = mid;
    }
    return hi;
}

static void insert(struct pass *out, unsigned *n, unsigned want, const struct pass *p)
{
    unsigned i = *n;
    if (i == want) {
        if (p->aos >= out[want - 1].aos) return;
        i = want - 1;
    } else {
        (*n)++;
    }
    while (i && out[i - 1].aos > p->aos) { out[i] = out[i - 1]; i--; }
    out[i] = *p;
}

unsigned orbit_passes(const struct sat *sats, unsigned count, const struct gmst_ref *g, const struct observer *o,
                      uint32_t now, unsigned hours, unsigned min_el_deg, struct pass *out, unsigned want)
{
    enum { STEP = 30 };
    unsigned found = 0;
    uint32_t end = now + hours * 3600u;
    for (unsigned k = 0; k < count; k++) {
        const struct sat *s = &sats[k];
        struct look l;
        orbit_look(s, g, o, now, &l);
        int up = l.el >= 0;
        struct pass p = { (uint8_t)k, 0, 0, 0, now, 0, now };
        int32_t best = l.el;
        if (up) p.az_aos = (uint16_t)orbit_deg(l.az);
        for (uint32_t t = now + STEP; t <= end; t += STEP) {
            if (found == want && !up && t >= out[want - 1].aos) break;  /* cannot make the list */
            orbit_look(s, g, o, t, &l);
            int now_up = l.el >= 0;
            if (now_up && !up) {
                p.aos = crossing(s, g, o, t - STEP, t, 0);
                struct look a;
                orbit_look(s, g, o, p.aos, &a);
                p.az_aos = (uint16_t)orbit_deg(a.az);
                best = l.el;
                p.tca = t;
            }
            if (now_up && l.el > best) { best = l.el; p.tca = t; }
            if (!now_up && up) {
                p.los = crossing(s, g, o, t - STEP, t, 1);
                struct look b;
                orbit_look(s, g, o, p.los, &b);
                p.az_los = (uint16_t)orbit_deg(b.az);
                /* The 30 s samples miss the peak of high passes; a low-orbit
                 * pass is symmetric in time to within seconds, so look at
                 * the midpoint (a pass already up at `now` keeps its best
                 * sample). */
                struct look peak;
                uint32_t mid = p.aos + (p.los - p.aos) / 2;
                orbit_look(s, g, o, mid, &peak);
                if (peak.el > best) { best = peak.el; p.tca = mid; }
                unsigned deg = orbit_deg((uint32_t)best);
                p.max_el = (uint8_t)(deg > 90 ? 0 : deg);
                if (p.max_el >= min_el_deg) insert(out, &found, want, &p);
                best = l.el;
            }
            up = now_up;
        }
    }
    return found;
}
