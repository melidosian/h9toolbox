#ifndef H9_MAIN_CLOCK_PROBE_H
#define H9_MAIN_CLOCK_PROBE_H

#include <stdbool.h>
#include <stdint.h>

#define H9_MAIN_CLOCK_PROBE_CHANNELS 4u

/* First part of stock clock helper app+0x0C54B2..0x0C5544. The rest of
 * that helper (clock selection/calibration) is not ported here. */
typedef struct {
    volatile uint32_t *fuse_word;
    volatile uint32_t *calibration_words;
    volatile uint32_t *measurement;
    void *context;
    void (*rom_write)(void *context, uint32_t selector, uint32_t value);
    uint32_t (*rom_read)(void *context, uint32_t selector);
    void (*rom_mask)(void *context, uint32_t selector, uint32_t mask);
    bool (*measurement_complete)(void *context, volatile uint32_t *measurement);
    uint32_t poll_limit;
} H9MainClockProbe;

void h9_main_clock_probe_bind_target(H9MainClockProbe *probe,
                                     uint32_t poll_limit);
bool h9_main_clock_probe_run(H9MainClockProbe *probe);

#endif
