#ifndef H9_RESIDENT_APRS_RUNTIME_H
#define H9_RESIDENT_APRS_RUNTIME_H
#include "beacon_service.h"
#include "../modules/common/toolbox_settings_pair.h"

#define H9_RUNTIME_RAM 0x29A00u
#define H9_RUNTIME_BYTES 0x200u
#ifdef H9_RESIDENT_TX
#define H9_RUNTIME_ABI 2u
#else
#define H9_RUNTIME_ABI 1u
#endif

struct h9_runtime_status {
    struct h9_bs_status beacon;
    struct ts_settings settings;
    uint32_t polls;
    uint8_t store_result;
    uint8_t worker_ready;
    uint8_t uart_online;
};

/* Assembly and modules can use this prefix even if the far code is absent. */
struct h9_runtime_prefix {
    const struct h9_runtime_api *api;
    volatile uint8_t mute;
    uint8_t inspected;
};

struct h9_runtime_api {
    uint32_t abi;
    unsigned (*read_status)(struct h9_runtime_status *);
    /* Called from the Toolbox UI in my_app, after a verified journal write. */
    void (*publish_settings)(const struct ts_settings *);
#ifdef H9_RESIDENT_TX
    void (*normal_tx)(void);
    unsigned (*prepare_tx)(const void *);
    unsigned (*emit_frame)(void);
    unsigned (*airtime)(void);
    void (*stock_tx)(const void *, unsigned, void (*)(const void *, unsigned));
#endif
};

#endif
