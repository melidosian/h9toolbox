#include "h9_main_aprs.h"

#include <stddef.h>

static bool valid_side(H9Side side)
{
    return side == H9_SIDE_A || side == H9_SIDE_B;
}

uint8_t h9_main_track_side_bit(uint8_t side_state, bool compare_working,
                                uint32_t working_frequency,
                                uint32_t a_frequency, uint32_t b_frequency)
{
    if (!compare_working) return side_state;
    if (working_frequency == a_frequency)
        return (uint8_t)(side_state & 0x7Fu);
    if (working_frequency == b_frequency)
        return (uint8_t)(side_state | 0x80u);
    return side_state;
}

H9Side h9_main_side_from_state(uint8_t side_state)
{
    return (side_state & 0x80u) != 0 ? H9_SIDE_B : H9_SIDE_A;
}

H9Side h9_main_rf_setup_side(bool dual_watch, uint8_t route_side_byte,
                             uint8_t channel_side_state)
{
    return dual_watch ?
        (route_side_byte != 0u ? H9_SIDE_B : H9_SIDE_A) :
        h9_main_side_from_state(channel_side_state);
}

H9AudioPlan h9_main_audio_plan(const H9MainAprsConfig *config,
                               H9Side received_side)
{
    H9AudioPlan result = { false, true, true };
    bool selected;
    if (config == NULL || !valid_side(received_side)) return result;
    selected = config->aprs_enabled &&
        ((config->rx_choice == H9_RX_A && received_side == H9_SIDE_A) ||
         (config->rx_choice == H9_RX_B && received_side == H9_SIDE_B));
    result.decoder_feed = selected;
    if (selected && config->mute_aprs_audio) {
        result.speaker_audio = false;
        result.bluetooth_audio = false;
    }
    return result;
}

bool h9_main_rx_channel_matches(H9RxChoice choice, uint32_t working_rx_hz,
                                uint32_t a_rx_hz, uint32_t b_rx_hz)
{
    H9Side working_side;
    if (choice == H9_RX_OFF) return true;
    if (choice != H9_RX_A && choice != H9_RX_B) return false;
    if (working_rx_hz == a_rx_hz) working_side = H9_SIDE_A;
    else if (working_rx_hz == b_rx_hz) working_side = H9_SIDE_B;
    else return false;
    return (choice == H9_RX_A && working_side == H9_SIDE_A) ||
           (choice == H9_RX_B && working_side == H9_SIDE_B);
}

H9TxPlan h9_main_tx_plan(const H9MainAprsConfig *config, H9Side current_side,
                         uint32_t a_tx_hz, uint32_t b_tx_hz)
{
    H9TxPlan result = { false, H9_SIDE_A, 0, false };
    H9Side chosen;
    if (config == NULL || !config->aprs_enabled || !valid_side(current_side))
        return result;
    switch (config->tx_choice) {
    case H9_TX_A_LOCK:
        if (current_side != H9_SIDE_A) return result;
        chosen = H9_SIDE_A;
        break;
    case H9_TX_B_LOCK:
        if (current_side != H9_SIDE_B) return result;
        chosen = H9_SIDE_B;
        break;
    case H9_TX_A_AUTO:
        chosen = H9_SIDE_A;
        break;
    case H9_TX_B_AUTO:
        chosen = H9_SIDE_B;
        break;
    case H9_TX_CURRENT:
        chosen = current_side;
        break;
    default:
        return result;
    }
    result.expected_tx_hz = chosen == H9_SIDE_A ? a_tx_hz : b_tx_hz;
    if (result.expected_tx_hz == 0) return result;
    result.side = chosen;
    result.allowed = true;
    result.restore_side_after_send = chosen != current_side &&
        (config->tx_choice == H9_TX_A_AUTO ||
         config->tx_choice == H9_TX_B_AUTO);
    return result;
}

bool h9_main_tx_prepared(const H9TxPlan *plan, H9Side programmed_side,
                         uint32_t programmed_tx_hz)
{
    return plan != NULL && plan->allowed && valid_side(programmed_side) &&
        programmed_side == plan->side && programmed_tx_hz != 0 &&
        programmed_tx_hz == plan->expected_tx_hz;
}
