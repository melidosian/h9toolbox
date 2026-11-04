#ifndef H9_MAIN_CLOCK_APPLY_H
#define H9_MAIN_CLOCK_APPLY_H

#include <stdbool.h>
#include <stdint.h>

/* First stock clock-apply sequence at main app+0x0C558A..0x0C55A2. */
typedef struct {
    volatile uint32_t *control_1e000c;
    void *context;
    void (*rom_apply)(void *context);
    void (*synchronize)(void *context);
    void (*delay_nops)(void *context, uint32_t count);
} H9MainClockApply;

void h9_main_clock_apply_bind_target(H9MainClockApply *apply);
bool h9_main_clock_apply_first(H9MainClockApply *apply);

#endif
