#ifndef H9_MAIN_CLOCK_APPLY_H
#define H9_MAIN_CLOCK_APPLY_H

#include <stdbool.h>
#include <stdint.h>

#include "h9_main_clock_probe.h"

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

/* Remaining stock clock-helper body at app+0x0C55AA..0x0C563E. The source
 * and auxiliary helpers are the separately ported app+0x0C52D4,
 * app+0x0C5308, and app+0x0C5330 routines. */
typedef struct {
    volatile uint32_t *control_1e000c;
    H9MainClockRegisters *source;
    H9MainClockAuxFields *auxiliary;
    void *context;
    void (*delay_nops)(void *context, uint32_t count);
    void (*rom_finish)(void *context);
} H9MainClockFinish;

void h9_main_clock_finish_bind_target(H9MainClockFinish *finish,
                                      H9MainClockRegisters *source,
                                      H9MainClockAuxFields *auxiliary);
bool h9_main_clock_finish_run(H9MainClockFinish *finish);

#endif
