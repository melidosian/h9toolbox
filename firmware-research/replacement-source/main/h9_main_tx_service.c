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
    H9MainTxResult result;
    bool stopped;
    bool restored;

    if (config == NULL || packet == NULL || length == 0u ||
        length > H9_MAIN_AX25_MAX_PACKET || hardware == NULL ||
        hardware->read_current_side == NULL ||
        hardware->validate_frequency == NULL ||
        hardware->prepare_tx == NULL ||
        hardware->read_prepared_tx == NULL || hardware->send_packet == NULL ||
        hardware->stop_tx == NULL || hardware->restore_receive == NULL ||
        !hardware->read_current_side(opaque, &previous_side))
        return H9_MAIN_TX_REJECTED;

    plan = h9_main_tx_plan(config, previous_side, a_tx_hz, b_tx_hz);
    if (!plan.allowed) return H9_MAIN_TX_REJECTED;
    if (!hardware->validate_frequency(opaque, plan.expected_tx_hz))
        return H9_MAIN_TX_REJECTED;

    result = H9_MAIN_TX_PREPARE_FAILED;
    if (hardware->prepare_tx(opaque, plan.side, plan.expected_tx_hz)) {
        result = H9_MAIN_TX_READBACK_FAILED;
        if (hardware->read_prepared_tx(opaque, &prepared_side, &prepared_hz) &&
            h9_main_tx_prepared(&plan, prepared_side, prepared_hz)) {
            result = hardware->send_packet(opaque, packet, length) ?
                     H9_MAIN_TX_SENT : H9_MAIN_TX_SEND_FAILED;
        }
    }

    stopped = hardware->stop_tx(opaque);
    restored = hardware->restore_receive(opaque, previous_side);
    if (!stopped || !restored) return H9_MAIN_TX_CLEANUP_FAILED;
    return result;
}
