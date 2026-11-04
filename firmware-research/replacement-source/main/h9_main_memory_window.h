#ifndef H9_MAIN_MEMORY_WINDOW_H
#define H9_MAIN_MEMORY_WINDOW_H

#include <stdbool.h>
#include <stdint.h>

#define H9_MAIN_MEMORY_WINDOW_BYTES 0x1200u
#define H9_MAIN_MEMORY_STATE_BYTES 0xE0u

/* One-time board memory-window stage at stock app+0x05FB64..0x05FBF6.
 * Caller must invoke this only when the preceding 0xD5F0 guard was zero. */
typedef struct {
    volatile uint32_t *controller;
    volatile uint8_t *window;
    volatile uint8_t *state;
} H9MainMemoryWindow;

void h9_main_memory_window_bind_target(H9MainMemoryWindow *window);
bool h9_main_memory_window_initialize(H9MainMemoryWindow *window);

#endif
