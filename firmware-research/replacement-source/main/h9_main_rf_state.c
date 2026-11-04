#include "h9_main_rf_state.h"

#include <stddef.h>

void h9_main_rf_state_bind_target(H9MainRfStateMemory *memory)
{
    if (memory == NULL) return;
    memory->dual_watch = (const volatile uint8_t *)(uintptr_t)0x12BA7u;
    memory->route_side = (volatile uint8_t *)(uintptr_t)0x11AECu;
    memory->channel_side_state = (volatile uint8_t *)(uintptr_t)0x11B50u;
    memory->working_rx_10hz = (const volatile uint32_t *)(uintptr_t)0x11C8Cu;
    memory->working_tx_10hz = (const volatile uint32_t *)(uintptr_t)0x11C9Cu;
    memory->a_rx_10hz = (const volatile uint32_t *)(uintptr_t)0x11CA0u;
    memory->a_tx_10hz = (const volatile uint32_t *)(uintptr_t)0x11CA4u;
    memory->b_rx_10hz = (const volatile uint32_t *)(uintptr_t)0x11CA8u;
    memory->b_tx_10hz = (const volatile uint32_t *)(uintptr_t)0x11CACu;
}

bool h9_main_rf_state_read(const H9MainRfStateMemory *memory,
                           H9MainRfSnapshot *snapshot)
{
    uint8_t dual_watch;
    uint8_t route_side;
    uint8_t side_state;
    H9MainRfSnapshot value;
    if (memory == NULL || snapshot == NULL ||
        memory->dual_watch == NULL || memory->route_side == NULL ||
        memory->channel_side_state == NULL ||
        memory->working_rx_10hz == NULL || memory->working_tx_10hz == NULL ||
        memory->a_rx_10hz == NULL || memory->a_tx_10hz == NULL ||
        memory->b_rx_10hz == NULL || memory->b_tx_10hz == NULL)
        return false;
    dual_watch = *memory->dual_watch;
    route_side = *memory->route_side;
    side_state = *memory->channel_side_state;
    value.working.receive_10hz = *memory->working_rx_10hz;
    value.working.transmit_10hz = *memory->working_tx_10hz;
    value.a.receive_10hz = *memory->a_rx_10hz;
    value.a.transmit_10hz = *memory->a_tx_10hz;
    value.b.receive_10hz = *memory->b_rx_10hz;
    value.b.transmit_10hz = *memory->b_tx_10hz;
    if (dual_watch != *memory->dual_watch ||
        route_side != *memory->route_side ||
        side_state != *memory->channel_side_state)
        return false;
    value.dual_watch = dual_watch != 0u;
    value.logical_side = h9_main_side_from_state(side_state);
    value.setup_side = h9_main_rf_setup_side(value.dual_watch, route_side,
                                             side_state);
    *snapshot = value;
    return true;
}

bool h9_main_rf_route_prepare(const H9MainRfStateMemory *memory,
                              const H9TxPlan *plan,
                              H9MainRfRouteSaved *saved)
{
    uint8_t dual_watch;
    uint8_t route_side;
    uint8_t side_state;
    if (memory == NULL || plan == NULL || saved == NULL || saved->active ||
        memory->dual_watch == NULL || memory->route_side == NULL ||
        memory->channel_side_state == NULL || !plan->allowed ||
        plan->expected_tx_hz == 0u ||
        (plan->side != H9_SIDE_A && plan->side != H9_SIDE_B)) return false;
    dual_watch = *memory->dual_watch;
    route_side = *memory->route_side;
    side_state = *memory->channel_side_state;
    if (!plan->restore_side_after_send &&
        h9_main_side_from_state(side_state) != plan->side) return false;
    saved->route_side = route_side;
    saved->channel_side_state = side_state;
    if (plan->restore_side_after_send)
        *memory->channel_side_state = (uint8_t)((side_state & 0x7Fu) |
            (plan->side == H9_SIDE_B ? 0x80u : 0u));
    if (dual_watch != 0u) {
        if (plan->side == H9_SIDE_A && route_side != 0u)
            *memory->route_side = 0u;
        else if (plan->side == H9_SIDE_B && route_side == 0u)
            *memory->route_side = 1u;
    }
    if (dual_watch != *memory->dual_watch ||
        h9_main_side_from_state(*memory->channel_side_state) != plan->side ||
        h9_main_rf_setup_side(dual_watch, *memory->route_side,
                              *memory->channel_side_state) != plan->side) {
        *memory->route_side = route_side;
        *memory->channel_side_state = side_state;
        return false;
    }
    saved->active = true;
    return true;
}

bool h9_main_rf_route_restore(const H9MainRfStateMemory *memory,
                              H9MainRfRouteSaved *saved)
{
    if (memory == NULL || saved == NULL || !saved->active ||
        memory->route_side == NULL || memory->channel_side_state == NULL)
        return false;
    *memory->route_side = saved->route_side;
    *memory->channel_side_state = saved->channel_side_state;
    saved->active = false;
    return *memory->route_side == saved->route_side &&
           *memory->channel_side_state == saved->channel_side_state;
}

bool h9_main_rf_snapshot_matches_plan(const H9MainRfSnapshot *snapshot,
                                      const H9MainAprsConfig *config,
                                      const H9TxPlan *plan)
{
    const H9MainRfChannelFrequencies *selected;
    if (snapshot == NULL || config == NULL || plan == NULL ||
        !config->aprs_enabled || !plan->allowed ||
        snapshot->setup_side != plan->side) return false;
    switch (config->tx_choice) {
    case H9_TX_A_LOCK:
        if (plan->side != H9_SIDE_A ||
            snapshot->logical_side != H9_SIDE_A) return false;
        break;
    case H9_TX_B_LOCK:
        if (plan->side != H9_SIDE_B ||
            snapshot->logical_side != H9_SIDE_B) return false;
        break;
    case H9_TX_A_AUTO:
        if (plan->side != H9_SIDE_A) return false;
        break;
    case H9_TX_B_AUTO:
        if (plan->side != H9_SIDE_B) return false;
        break;
    case H9_TX_CURRENT:
        if (snapshot->logical_side != plan->side) return false;
        break;
    default:
        return false;
    }
    selected = plan->side == H9_SIDE_A ? &snapshot->a : &snapshot->b;
    return selected->receive_10hz != 0u &&
           selected->transmit_10hz != 0u &&
           snapshot->working.receive_10hz == selected->receive_10hz &&
           snapshot->working.transmit_10hz == selected->transmit_10hz;
}
