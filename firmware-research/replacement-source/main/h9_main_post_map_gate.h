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

/* Stock helper app+0x05E37E..0x05E39E, called with mode 1 at
 * app+0x05FF9A. The mode-0 branch additionally writes word +8. */
typedef struct {
    H9MainClockHandshake *handshake;
    volatile uint32_t *words_10f0d8;
} H9MainPostMapWindow;

void h9_main_post_map_window_bind_target(H9MainPostMapWindow *window,
                                          H9MainClockHandshake *handshake);
bool h9_main_post_map_window_run(H9MainPostMapWindow *window,
                                  uint32_t mode, uint32_t word_e4,
                                  uint32_t word_e8);

#endif
