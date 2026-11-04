#include "h9_main_tx_service.h"

#include "h9_main_ax25.h"

H9MainTxResult h9_main_tx_send(const H9MainAprsConfig *config,
                               uint32_t a_tx_hz, uint32_t b_tx_hz,
                               const uint8_t *packet, size_t length,
                               const H9MainTxHardware *hardware, void *opaque)
{
    H9Side previous_side;
    H9Side prepared_side;
    uint32_t prepared_hz;
    H9TxPlan plan;
    H9MainRfSnapshot rf_snapshot;
    H9BeaconPositionStatus position;
    H9MainTxResult result;
    bool stopped;
    bool permit_cleared;
    bool restored;

    if (config == NULL || packet == NULL || length == 0u ||
        length > H9_MAIN_AX25_MAX_PACKET || hardware == NULL ||
        hardware->begin_route == NULL || hardware->end_route == NULL ||
        hardware->read_current_side == NULL ||
        hardware->read_position_status == NULL ||
        hardware->validate_frequency == NULL ||
        hardware->prepare_tx == NULL ||
        hardware->read_prepared_tx == NULL ||
        hardware->read_rf_snapshot == NULL ||
        hardware->set_aprs_permit == NULL || hardware->send_packet == NULL ||
        hardware->stop_tx == NULL || hardware->restore_receive == NULL)
        return H9_MAIN_TX_REJECTED;

    if (!hardware->begin_route(opaque)) return H9_MAIN_TX_ROUTE_BUSY;
    result = H9_MAIN_TX_REJECTED;
    if (!hardware->read_current_side(opaque, &previous_side)) goto release;

    if (!hardware->read_position_status(opaque, &position) ||
        !h9_main_beacon_position_ready(&position)) goto release;

    plan = h9_main_tx_plan(config, previous_side, a_tx_hz, b_tx_hz);
    if (!plan.allowed) goto release;
    if (!hardware->validate_frequency(opaque, plan.expected_tx_hz))
        goto release;

    result = H9_MAIN_TX_PREPARE_FAILED;
    if (hardware->prepare_tx(opaque, &plan)) {
        result = H9_MAIN_TX_READBACK_FAILED;
        if (hardware->read_prepared_tx(opaque, &prepared_side, &prepared_hz) &&
            h9_main_tx_prepared(&plan, prepared_side, prepared_hz)) {
            result = H9_MAIN_TX_ROUTE_FAILED;
            if (hardware->read_rf_snapshot(opaque, &rf_snapshot) &&
                h9_main_rf_snapshot_matches_plan(&rf_snapshot, config,
                                                 &plan)) {
                result = H9_MAIN_TX_PERMIT_FAILED;
                if (hardware->set_aprs_permit(opaque, true))
                    result = hardware->send_packet(opaque, packet, length) ?
                             H9_MAIN_TX_SENT : H9_MAIN_TX_SEND_FAILED;
            }
        }
    }

    stopped = hardware->stop_tx(opaque);
    permit_cleared = hardware->set_aprs_permit(opaque, false);
    restored = hardware->restore_receive(opaque, previous_side);
    if (!stopped || !permit_cleared || !restored)
        result = H9_MAIN_TX_CLEANUP_FAILED;
release:
    if (!hardware->end_route(opaque)) return H9_MAIN_TX_CLEANUP_FAILED;
    return result;
}
