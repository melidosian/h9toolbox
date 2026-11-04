#ifndef H9_MAIN_BOOT_VALIDATOR_H
#define H9_MAIN_BOOT_VALIDATOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "h9_main_boot_record.h"

#define H9_MAIN_BOOT_DESCRIPTOR 0x00012850u

typedef struct {
    volatile uint32_t *stream_data;
    volatile uint32_t *stream_control;
    volatile uint32_t *result_word;
    void (*begin)(void *context, uint32_t descriptor, uint32_t mode);
    void (*synchronize)(void *context);
    void (*finish)(void *context, uint32_t descriptor);
    void *context;
} H9MainBootValidator;

/* Stock app+0x04CAEC streams 78 record bytes to MMIO 0x001E3500,
 * synchronizes, captures the word at 0x12854 before ROM cleanup, and
 * returns its low halfword. Zero indicates unavailable/invalid. */
uint32_t h9_main_boot_validate(const volatile uint8_t *data, size_t length,
                               void *context);

/* Available only in a Pi32 target build; host tests use injected registers
 * and callbacks. This does not connect the validator to the reset path. */
bool h9_main_boot_validator_bind_target(H9MainBootValidator *validator);

#endif
