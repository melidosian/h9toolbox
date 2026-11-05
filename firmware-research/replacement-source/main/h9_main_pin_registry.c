#include "h9_main_pin_registry.h"

#include <stddef.h>

_Static_assert(sizeof(H9MainPinRecord) == 8u, "stock pin record stride");

void h9_main_pin_registry_bind_target(H9MainPinRegistry *registry)
{
    if (registry == NULL) return;
    registry->records = (volatile H9MainPinRecord *)(uintptr_t)0x00012A30u;
}

H9MainPinResult h9_main_pin_registry_configure(H9MainPinRegistry *registry,
                                                uint32_t pin, int32_t key)
{
    unsigned index;
    volatile H9MainPinRecord *record;
    if (registry == NULL || registry->records == NULL ||
        registry->action_0176 == NULL || registry->action_01f6 == NULL ||
        registry->action_0284 == NULL || registry->action_02dc == NULL)
        return H9_MAIN_PIN_INVALID;

    registry->action_0176(registry->context, pin, 0u);
    registry->action_01f6(registry->context, pin, 1u);
    registry->action_0284(registry->context, pin, 0u);
    registry->action_02dc(registry->context, pin, 0u);
    for (index = 0u; index < H9_MAIN_PIN_RECORD_COUNT; ++index) {
        record = &registry->records[index];
        if (record->key == key) return H9_MAIN_PIN_EXISTING;
        if (record->key == -1) {
            record->key = key;
            record->enabled = 1u;
            return H9_MAIN_PIN_INSERTED;
        }
    }
    return H9_MAIN_PIN_FULL;
}

void h9_main_my_app_configure_early_pins(H9MainPinRegistry *registry)
{
    static const struct { uint8_t pin; int8_t key; } calls[] = {
        { 12u, 4 }, { 37u, 13 }, { 36u, 10 }, { 10u, 3 }, { 1u, 0 }
    };
    unsigned index;
    for (index = 0u; index < sizeof(calls) / sizeof(calls[0]); ++index)
        (void)h9_main_pin_registry_configure(registry,
                                              calls[index].pin,
                                              calls[index].key);
}
