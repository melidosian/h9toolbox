#ifndef H9_MAIN_BOARD_MARKERS_H
#define H9_MAIN_BOARD_MARKERS_H

#include <stdbool.h>
#include <stdint.h>

#define H9_MAIN_GUARD_BYTES 16u

/* Stock board entry app+0x05FBF6..0x05FC20. These writes occur whether
 * the earlier allocation-bitmap one-time guard was set or clear. */
typedef struct {
    volatile uint8_t *ram_code_guard;
    volatile uint8_t *stack_guard;
    volatile uint8_t *state_da58;
    volatile uint8_t *state_da5c;
    volatile uint8_t *state_d9d0;
} H9MainBoardMarkers;

void h9_main_board_markers_bind_target(H9MainBoardMarkers *markers);
bool h9_main_board_markers_initialize(H9MainBoardMarkers *markers);

#endif
