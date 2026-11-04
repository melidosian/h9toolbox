#ifndef H9_MAIN_GPS_FIX_H
#define H9_MAIN_GPS_FIX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    H9_MAIN_GPS_UNRECOGNIZED = 0,
    H9_MAIN_GPS_RMC,
    H9_MAIN_GPS_GGA
} H9MainGpsSentenceType;

/* Signed degrees times 10^7. Values are committed only from checksum-valid
 * sentences. A no-fix sentence clears has_position to avoid stale locations. */
typedef struct {
    int32_t latitude_e7;
    int32_t longitude_e7;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    bool has_position;
    bool has_date;
    bool has_time;
} H9MainGpsFix;

/* Accepts complete $..*HH\n or $..*HH\r\n RMC/GGA sentences. On failure,
 * leaves fix unchanged. This is replacement policy; stock side effects are
 * still incompletely recovered. */
H9MainGpsSentenceType h9_main_gps_parse_fix(const uint8_t *sentence,
                                             size_t length,
                                             H9MainGpsFix *fix);

#endif
