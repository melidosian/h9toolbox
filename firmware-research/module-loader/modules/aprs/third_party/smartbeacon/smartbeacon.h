/*
 * smartbeacon.h - speed/turn driven APRS beacon scheduler.
 *
 * Portable, hardware independent core of the TD-H9 smart beaconing module.
 * The caller feeds GNSS state once per second and transmits a position
 * report whenever sb_tick() returns 1.
 */
#ifndef SMARTBEACON_H
#define SMARTBEACON_H

#include <stdint.h>

typedef struct {
    uint32_t boot_s;       /* no beacon before this uptime                 */
    uint32_t guard_s;      /* minimum gap between any two transmissions    */
    uint32_t park_s;       /* interval when parked or without fix          */
    uint32_t fast_s;       /* shortest interval at high speed              */
    uint16_t park_kmh10;   /* below this speed [0.1 km/h] = parked         */
    uint32_t dist_k;       /* interval = dist_k / speed  (~distance step)  */
    uint16_t turn_min;     /* corner pegging: base angle [deg]             */
    uint32_t turn_slope;   /* corner pegging: angle += slope / speed       */
    uint32_t turn_time_s;  /* minimum gap for a corner-triggered beacon    */
    uint32_t stop_s;       /* one final beacon after this long stopped     */
    uint32_t fix_tmo_s;    /* fix considered lost after this silence       */
} sb_config_t;

typedef struct {
    uint8_t  fix;          /* last valid RMC had status 'A'   */
    uint16_t kmh10;        /* speed over ground [0.1 km/h]    */
    uint16_t course;       /* course over ground [deg]        */
    uint32_t fix_ms;       /* time of last valid RMC          */
} sb_gnss_t;

typedef struct {
    sb_config_t cfg;
    sb_gnss_t   gnss;
    uint32_t    last_tx_ms;
    uint16_t    tx_course;
    uint8_t     fix_prev, fix_pend, moving;
    uint32_t    park_ms;
} sb_state_t;

/* Field-tested defaults: ~300 m between beacons, 60 s .. 10 min. */
void sb_init(sb_state_t *s, const sb_config_t *cfg);
extern const sb_config_t SB_DEFAULTS;

/* Update GNSS state from a parsed RMC sentence. */
void sb_on_rmc(sb_state_t *s, uint32_t now_ms, uint8_t valid,
               uint16_t kmh10, uint16_t course);

/* Call about once per second. tx_busy = modem currently transmitting.
 * Returns 1 when a position beacon should be sent now. */
int sb_tick(sb_state_t *s, uint32_t now_ms, int tx_busy);

#endif
