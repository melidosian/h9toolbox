#include "h9_main_tx_adapter.h"
#include "h9_aprs_tx_permit_protocol.h"

#include <stddef.h>

static H9MainTxAdapter *adapter_from(void *opaque)
{
    return (H9MainTxAdapter *)opaque;
}

static bool begin_route(void *opaque)
{
    H9MainTxAdapter *adapter = adapter_from(opaque);
    if (adapter->reserved ||
        !adapter->board.route_acquire(adapter->board.context)) return false;
    adapter->reserved = true;
    adapter->stop_confirmed = true;
    adapter->receive_restored = true;
    return true;
}

static bool end_route(void *opaque)
{
    H9MainTxAdapter *adapter = adapter_from(opaque);
    bool released;
    if (!adapter->reserved || !adapter->stop_confirmed ||
        !adapter->receive_restored) return false;
    released = adapter->board.route_release(adapter->board.context);
    if (released) adapter->reserved = false;
    return released;
}

static bool read_current_side(void *opaque, H9Side *side)
{
    H9MainTxAdapter *adapter = adapter_from(opaque);
    if (!adapter->reserved || adapter->route.channel_side_state == NULL ||
        side == NULL) return false;
    *side = h9_main_side_from_state(*adapter->route.channel_side_state);
    return true;
}

static bool read_position(void *opaque, H9BeaconPositionStatus *status)
{
    H9MainTxAdapter *adapter = adapter_from(opaque);
    return adapter->reserved &&
           h9_main_tx_ram_policy_read_position(&adapter->policy, status);
}

static bool frequency_allowed(void *opaque, uint32_t tx_hz)
{
    H9MainTxAdapter *adapter = adapter_from(opaque);
    return adapter->reserved &&
           h9_main_tx_ram_policy_frequency_allowed(&adapter->policy, tx_hz);
}

static bool prepare_tx(void *opaque, const H9TxPlan *plan)
{
    H9MainTxAdapter *adapter = adapter_from(opaque);
    if (!adapter->reserved) return false;
    adapter->receive_restored = false;
    return h9_main_rf_route_prepare(&adapter->route, plan,
                                    &adapter->saved_route) &&
           adapter->board.prepare_unkeyed(adapter->board.context, plan);
}

static bool read_prepared_tx(void *opaque, H9Side *side, uint32_t *tx_hz)
{
    H9MainTxAdapter *adapter = adapter_from(opaque);
    return adapter->reserved &&
           adapter->board.read_synth(adapter->board.context, side, tx_hz);
}

static bool read_rf_snapshot(void *opaque, H9MainRfSnapshot *snapshot)
{
    H9MainTxAdapter *adapter = adapter_from(opaque);
    return adapter->reserved &&
           h9_main_rf_state_read(&adapter->route, snapshot);
}

static bool wait_for(H9MainTxAdapter *adapter, H9MainAprsTxStatus wanted)
{
    uint32_t step;
    for (step = 0u; step < adapter->max_poll_steps; ++step) {
        H9MainAprsTxStatus status;
        (void)h9_main_aprs_uart1_link_poll(adapter->uart);
        status = h9_main_aprs_uart1_link_tx_status(adapter->uart,
            adapter->board.now_ms(adapter->board.context));
        if (status == wanted) return true;
        if (status == H9_MAIN_APRS_TX_TIMED_OUT ||
            status == H9_MAIN_APRS_TX_IDLE ||
            !adapter->board.yield(adapter->board.context)) return false;
    }
    return false;
}

static bool set_aprs_permit(void *opaque, bool enabled)
{
    H9MainTxAdapter *adapter = adapter_from(opaque);
    if (!adapter->reserved) return false;
    if (!enabled)
        return adapter->uart->tx_active_token == 0u ||
               h9_main_aprs_uart1_link_tx_clear(adapter->uart);
    if (h9_main_aprs_uart1_link_tx_arm(adapter->uart,
        adapter->board.now_ms(adapter->board.context),
        adapter->timeout_ms) == 0u) return false;
    return wait_for(adapter, H9_MAIN_APRS_TX_READY_TO_KEY);
}

static bool send_packet(void *opaque, const uint8_t *packet, size_t length)
{
    H9MainTxAdapter *adapter = adapter_from(opaque);
    if (!adapter->reserved ||
        h9_main_aprs_uart1_link_tx_status(adapter->uart,
            adapter->board.now_ms(adapter->board.context)) !=
            H9_MAIN_APRS_TX_READY_TO_KEY) return false;
    /* A failed key callback may have partially asserted RF. Cleanup must
     * confirm the stop before route restoration or reservation release. */
    adapter->stop_confirmed = false;
    if (!adapter->board.key_rf(adapter->board.context)) return false;
    if (!h9_main_aprs_uart1_link_tx_send(adapter->uart, packet, length,
        adapter->board.now_ms(adapter->board.context))) return false;
    return wait_for(adapter, H9_MAIN_APRS_TX_DRAINED);
}

static bool stop_tx(void *opaque)
{
    H9MainTxAdapter *adapter = adapter_from(opaque);
    if (!adapter->board.unkey_rf(adapter->board.context)) return false;
    adapter->stop_confirmed = true;
    return true;
}

static bool restore_receive(void *opaque, H9Side previous_side)
{
    H9MainTxAdapter *adapter = adapter_from(opaque);
    bool restored_rf;
    bool restored_route;
    if (!adapter->stop_confirmed) return false;
    restored_rf = adapter->board.restore_rf_receive(
        adapter->board.context, previous_side);
    restored_route = !adapter->saved_route.active ||
        h9_main_rf_route_restore(&adapter->route, &adapter->saved_route);
    adapter->receive_restored = restored_rf && restored_route;
    return adapter->receive_restored;
}

H9MainTxResult h9_main_tx_adapter_send(H9MainTxAdapter *adapter,
                                        const H9MainAprsConfig *config,
                                        uint32_t a_tx_hz, uint32_t b_tx_hz,
                                        const uint8_t *packet, size_t length)
{
    const H9MainTxHardware hardware = {
        begin_route, end_route, read_current_side, read_position,
        frequency_allowed, prepare_tx, read_prepared_tx, read_rf_snapshot,
        set_aprs_permit, send_packet, stop_tx, restore_receive
    };
    if (adapter == NULL || adapter->uart == NULL ||
        adapter->timeout_ms == 0u ||
        adapter->timeout_ms > H9_APRS_TX_PERMIT_LEASE_MS ||
        adapter->max_poll_steps == 0u ||
        adapter->board.route_acquire == NULL ||
        adapter->board.route_release == NULL ||
        adapter->board.prepare_unkeyed == NULL ||
        adapter->board.read_synth == NULL ||
        adapter->board.key_rf == NULL ||
        adapter->board.unkey_rf == NULL ||
        adapter->board.restore_rf_receive == NULL ||
        adapter->board.now_ms == NULL || adapter->board.yield == NULL)
        return H9_MAIN_TX_REJECTED;
    return h9_main_tx_send(config, a_tx_hz, b_tx_hz, packet, length,
                            &hardware, adapter);
}
