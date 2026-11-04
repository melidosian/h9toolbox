#ifndef H9_MAIN_CLOCK_HANDSHAKE_H
#define H9_MAIN_CLOCK_HANDSHAKE_H

#include <stdbool.h>
#include <stdint.h>

/* Stock board entry app+0x05FEAE..0x05FF36 alternates status-bit waits
 * around ten gate enables. The status helpers live at app+0x05E340/354. */
typedef struct {
    void *context;
    volatile uint32_t *status_101044;
    volatile uint32_t *gates_101000;
    volatile uint32_t *control_10f0d0;
    volatile uint32_t *control_10f1c0;
    uint32_t (*read_status)(void *context, volatile uint32_t *status);
    uint32_t poll_limit;
    volatile uint32_t *window_words_101070;
} H9MainClockHandshake;

void h9_main_clock_handshake_bind_target(H9MainClockHandshake *handshake,
                                          uint32_t poll_limit);
bool h9_main_clock_handshake_run(H9MainClockHandshake *handshake);
bool h9_main_clock_wait(H9MainClockHandshake *handshake, bool set);
bool h9_main_clock_map_sources(H9MainClockHandshake *handshake,
                                uintptr_t app_entry, uintptr_t ram_code_source);

#endif
