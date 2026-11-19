/* Two-sector settings format. The board supplies the owned sectors and IO. */
#ifndef TOOLBOX_SETTINGS_PAIR_H
#define TOOLBOX_SETTINGS_PAIR_H
#include <stdint.h>

#define TS_DEFAULT_MINUTES 5u

struct ts_settings {
    uint32_t tx_frequency;  /* configured APRS transmit frequency in 10 Hz steps */
    uint8_t mute, armed, timed, minutes, symbol;
    uint8_t tx_setting, tx_side;
};

struct tsp_io {
    void *ctx;
    int (*read)(void *, uint32_t, void *, unsigned);
    int (*program)(void *, uint32_t, const void *, unsigned);
    int (*erase)(void *, uint32_t);
    uint32_t sector[2];
    unsigned ownership_approved;
};

enum tsp_result {
    TSP_OK, TSP_EMPTY, TSP_DENIED, TSP_FOREIGN, TSP_IO,
    TSP_INVALID, TSP_EXHAUSTED, TSP_DEGRADED
};

/* Check the GAME directory before claiming either sector. An interrupted
 * upload's valid allocation still owns its ranges. */
enum tsp_result tsp_check_game_directory(const struct tsp_io *, uint32_t directory);

/* A valid snapshot with an unreadable peer is returned as TSP_DEGRADED.
 * TSP_DEGRADED can provide the last mute preference. Resume automatic TX
 * only on TSP_OK. Empty/error defaults both features to OFF. */
enum tsp_result tsp_load(const struct tsp_io *, struct ts_settings *);
/* Every save requires exclusive reservation of BOTH whole sectors. Unknown
 * bytes or duplicate generations refuse writes. The prior committed sector
 * survives erase/program of the alternate sector. */
enum tsp_result tsp_save(const struct tsp_io *, const struct ts_settings *);

#endif
