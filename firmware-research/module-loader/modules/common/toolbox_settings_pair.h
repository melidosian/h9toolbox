/* Two-sector mute settings. No addresses are reserved or written by this API. */
#ifndef TOOLBOX_SETTINGS_PAIR_H
#define TOOLBOX_SETTINGS_PAIR_H
#include <stdint.h>

struct ts_settings { uint8_t mute; };

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
    TSP_INVALID, TSP_EXHAUSTED
};

/* A valid snapshot remains readable if the other sector is torn/foreign.
 * The caller must inspect the result: empty/error defaults mute to OFF. */
enum tsp_result tsp_load(const struct tsp_io *, struct ts_settings *);
/* Every save requires exclusive reservation of BOTH whole sectors. Unknown
 * bytes or duplicate generations refuse writes. The prior committed sector
 * survives erase/program of the alternate sector. */
enum tsp_result tsp_save(const struct tsp_io *, const struct ts_settings *);

#endif
