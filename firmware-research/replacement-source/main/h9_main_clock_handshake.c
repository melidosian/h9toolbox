#include "h9_main_clock_handshake.h"

#include <stddef.h>

static bool wait_for_status(H9MainClockHandshake *handshake, bool set)
{
    uint32_t poll;
    /* Both stock helpers read first. Only a failed read writes 0xE7. */
    for (poll = 0u; poll < handshake->poll_limit; ++poll) {
        uint32_t status = handshake->read_status(handshake->context,
                                                handshake->status_101044);
        if (((status & 1u) != 0u) == set) return true;
        *handshake->status_101044 = 0xE7u;
    }
    return false;
}

bool h9_main_clock_handshake_run(H9MainClockHandshake *handshake)
{
    static const uint32_t gate_masks[6] = {
        0x20000000u, 0x10000000u, 0x08000000u,
        0x04000000u, 0x02000000u, 0x01000000u
    };
    static const uint32_t control_masks[3] = { 1u, 2u, 4u };
    uint32_t stage;
    volatile uint32_t *target;
    uint32_t mask;
    if (handshake == NULL || handshake->status_101044 == NULL ||
        handshake->gates_101000 == NULL ||
        handshake->control_10f0d0 == NULL ||
        handshake->control_10f1c0 == NULL ||
        handshake->read_status == NULL || handshake->poll_limit == 0u)
        return false;
    for (stage = 0u; stage < 10u; ++stage) {
        if (!wait_for_status(handshake, true)) return false;
        if (stage < 6u) {
            target = handshake->gates_101000;
            mask = gate_masks[stage];
        } else if (stage < 9u) {
            target = handshake->control_10f0d0;
            mask = control_masks[stage - 6u];
        } else {
            target = handshake->control_10f1c0;
            mask = 1u;
        }
        *target |= mask;
        if (!wait_for_status(handshake, false)) return false;
    }
    return true;
}

bool h9_main_clock_map_sources(H9MainClockHandshake *handshake,
                                uintptr_t app_entry, uintptr_t ram_code_source)
{
    volatile uint32_t *window;
    if (handshake == NULL || handshake->status_101044 == NULL ||
        handshake->window_words_101070 == NULL ||
        handshake->read_status == NULL || handshake->poll_limit == 0u ||
        app_entry == 0u || ram_code_source == 0u) return false;
    if (!wait_for_status(handshake, true)) return false;
    window = handshake->window_words_101070;
    window[1] = (uint32_t)app_entry;
    window[0] = (uint32_t)ram_code_source;
    /* The board entry at app+0x05FF46 reads all four words, in this order. */
    (void)window[1];
    (void)window[0];
    (void)window[3];
    (void)window[2];
    return true;
}

static uint32_t target_read_status(void *context, volatile uint32_t *status)
{
    (void)context;
    return *status;
}

void h9_main_clock_handshake_bind_target(H9MainClockHandshake *handshake,
                                          uint32_t poll_limit)
{
    if (handshake == NULL) return;
    handshake->context = NULL;
    handshake->status_101044 =
        (volatile uint32_t *)(uintptr_t)0x00101044u;
    handshake->gates_101000 =
        (volatile uint32_t *)(uintptr_t)0x00101000u;
    handshake->control_10f0d0 =
        (volatile uint32_t *)(uintptr_t)0x0010F0D0u;
    handshake->control_10f1c0 =
        (volatile uint32_t *)(uintptr_t)0x0010F1C0u;
    handshake->read_status = target_read_status;
    handshake->poll_limit = poll_limit;
    handshake->window_words_101070 =
        (volatile uint32_t *)(uintptr_t)0x00101070u;
}
