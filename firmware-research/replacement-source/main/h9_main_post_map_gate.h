#ifndef H9_MAIN_POST_MAP_GATE_H
#define H9_MAIN_POST_MAP_GATE_H

#include <stdbool.h>
#include <stdint.h>

#include "h9_main_clock_handshake.h"

/* Stock board entry app+0x05FF52..0x05FF9A initializes the reserved RAM
 * prefix and configures two control pairs through status handshakes. */
typedef struct {
    void *context;
    H9MainClockHandshake *handshake;
    bool (*fill_prefix_ones)(void *context);
    volatile uint32_t *config_101040;
    volatile uint32_t *words_101050;
} H9MainPostMapGate;

void h9_main_post_map_gate_bind_target(H9MainPostMapGate *gate,
                                        H9MainClockHandshake *handshake);
bool h9_main_post_map_gate_run(H9MainPostMapGate *gate);

#endif
