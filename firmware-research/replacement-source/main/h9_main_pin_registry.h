#ifndef H9_MAIN_PIN_REGISTRY_H
#define H9_MAIN_PIN_REGISTRY_H

#include <stdint.h>

/* Stock app+0x079BE4 calls four low-level pin helpers, then scans ten
 * eight-byte entries at RAM 0x12A30. The key is -1 for an unused entry. */
typedef struct {
    int32_t key;
    uint16_t enabled;
    uint16_t unknown_6;
} H9MainPinRecord;

enum { H9_MAIN_PIN_RECORD_COUNT = 10 };

typedef struct {
    void *context;
    void (*action_0176)(void *context, uint32_t pin, uint32_t value);
    void (*action_01f6)(void *context, uint32_t pin, uint32_t value);
    void (*action_0284)(void *context, uint32_t pin, uint32_t value);
    void (*action_02dc)(void *context, uint32_t pin, uint32_t value);
    volatile H9MainPinRecord *records;
} H9MainPinRegistry;

typedef enum {
    H9_MAIN_PIN_EXISTING,
    H9_MAIN_PIN_INSERTED,
    H9_MAIN_PIN_FULL,
    H9_MAIN_PIN_INVALID
} H9MainPinResult;

void h9_main_pin_registry_bind_target(H9MainPinRegistry *registry);
H9MainPinResult h9_main_pin_registry_configure(H9MainPinRegistry *registry,
                                                uint32_t pin, int32_t key);
/* First five calls in stock my_app at app+0x08818A..0x0881B2. */
void h9_main_my_app_configure_early_pins(H9MainPinRegistry *registry);

#endif
