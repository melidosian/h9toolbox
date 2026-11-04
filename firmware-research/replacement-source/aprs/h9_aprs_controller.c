#include "h9_aprs_controller.h"
#include "h9_aprs_identity.h"
#include "h9_aprs_burst_protocol.h"
#include "h9_aprs_tx_defaults.h"
#include "h9_aprs_tx_permit_protocol.h"
#include <string.h>

static bool send_frame(H9AprsController *controller, uint8_t command,
                       const uint8_t *data, size_t length)
{
    if (controller->hardware.uart_write == NULL) return false;
    if (h9_wire_encode_stream(command, data, length,
                              controller->hardware.uart_write,
                              controller->hardware.context) == 0)
        return false;
    return controller->hardware.uart_ok == NULL ||
           controller->hardware.uart_ok(controller->hardware.context);
}

static void forward_valid_frame(void *context, const uint8_t *data,
                                size_t length)
{
    H9AprsController *controller = (H9AprsController *)context;
    if (send_frame(controller, 0x00u, data, length))
        ++controller->forwarded_rx_frames;
}

static void send_version(H9AprsController *controller, uint8_t query)
{
    static const uint8_t module_version[8] = H9_APRS_MODULE_VERSION;
    static const uint8_t protocol_version[7] = H9_APRS_PROTOCOL_VERSION;
    uint8_t reply[16] = { 0 };
    size_t length;
    reply[0] = query;
    if (query == 0 || query == 1) {
        memcpy(reply + 1, module_version, sizeof module_version);
        length = 9;
        if (query == 0) {
            memcpy(reply + 9, protocol_version, sizeof protocol_version);
            length = 16;
        }
    } else if (query == 2) {
        memcpy(reply + 1, protocol_version, sizeof protocol_version);
        length = 8;
    } else {
        reply[0] = 0xFFu;
        length = 1;
    }
    (void)send_frame(controller, 0x21u, reply, length);
}

static void dispatch(H9AprsController *controller)
{
    const uint8_t *data = controller->wire.payload;
    size_t length = controller->wire.length;
    void *context = controller->hardware.context;
    switch (controller->wire.command) {
    case 0x00u:
        if (length != 0 && controller->hardware.transmit_aprs_packet != NULL)
            controller->hardware.transmit_aprs_packet(context, data, length);
        break;
    case 0x10u:
        if (length != 0) (void)send_frame(controller, 0x10u, data, length);
        break;
    case 0x11u:
        if (length == 0) break;
        switch (data[0]) {
        case 1:
            if (controller->hardware.set_radio_gpio != NULL)
                controller->hardware.set_radio_gpio(context, true);
            break;
        case 2:
            if (controller->hardware.set_radio_gpio != NULL)
                controller->hardware.set_radio_gpio(context, false);
            break;
        case 3:
            if (controller->hardware.read_radio_gpio != NULL) {
                uint8_t value = controller->hardware.read_radio_gpio(context) ? 1u : 0u;
                (void)send_frame(controller, 0x10u, &value, 1);
            }
            break;
        case 4:
            if (length > 1 && controller->hardware.read_radio_gpio != NULL &&
                controller->hardware.read_radio_gpio(context) &&
                controller->hardware.aux_uart_write != NULL)
                controller->hardware.aux_uart_write(context, data + 1, length - 1);
            break;
        default:
            break;
        }
        break;
    case 0x13u:
        if (controller->hardware.read_status != NULL) {
            uint8_t status[48];
            if (controller->hardware.read_status(context, status))
                (void)send_frame(controller, 0x13u, status, sizeof status);
        }
        break;
    case 0x20u:
        send_version(controller, length != 0 ? data[0] : 0);
        break;
    case H9_APRS_TX_PERMIT_COMMAND:
        if (length == 2u && data[0] == H9_APRS_TX_PERMIT_VERSION &&
            data[1] <= H9_APRS_TX_PERMIT_ENABLE &&
            controller->hardware.tx_permit_changed != NULL)
            controller->hardware.tx_permit_changed(context, data[1] != 0u);
        break;
    default:
        break;
    }
}

static void apply_raw_setting(H9AprsController *controller,
                              uint8_t command, uint8_t value)
{
    H9AprsTxSettings *settings = &controller->tx_settings;
    switch (command) {
    case 1u:
        settings->lead_ms = (uint16_t)value * 10u;
        controller->tx.lead_ms = settings->lead_ms;
        break;
    case 2u:
        settings->persistence = value;
        break;
    case 3u:
        settings->parameter3_ms = (uint16_t)value * 10u;
        break;
    case 4u:
        settings->tail_ms = (uint16_t)value * 10u;
        controller->tx.tail_ms = settings->tail_ms;
        break;
    case 0x0Fu:
        controller->tx_status_enabled = value != 0u;
        return;
    default:
        return;
    }
    if (controller->hardware.tx_settings_changed != NULL)
        controller->hardware.tx_settings_changed(controller->hardware.context,
                                                 settings);
}

void h9_aprs_controller_init(H9AprsController *controller,
                             const H9AprsHardware *hardware)
{
    if (controller == NULL) return;
    memset(controller, 0, sizeof *controller);
    if (hardware != NULL) controller->hardware = *hardware;
    h9_wire_reset(&controller->wire);
    h9_aprs_demod_init(&controller->demod);
    h9_aprs_frame_init(&controller->frame);
    controller->tx_settings.lead_ms = H9_APRS_STOCK_TX_LEAD_MS;
    controller->tx_settings.tail_ms = H9_APRS_STOCK_TX_TAIL_MS;
    controller->tx_settings.parameter3_ms = H9_APRS_STOCK_TX_PARAMETER3_MS;
    controller->tx_settings.persistence = H9_APRS_STOCK_TX_PERSISTENCE;
    h9_aprs_tx_init(&controller->tx, controller->tx_settings.lead_ms,
                    controller->tx_settings.tail_ms);
}

void h9_aprs_controller_uart_byte(H9AprsController *controller, uint8_t byte)
{
    H9WireResult result;
    bool setting_byte;
    if (controller == NULL) return;
    setting_byte = controller->wire.started && controller->wire.has_command &&
                   byte != H9_APRS_FRAME_END;
    result = h9_wire_feed(&controller->wire, byte);
    if (setting_byte && result == H9_WIRE_MORE &&
        controller->wire.length == 1u)
        apply_raw_setting(controller, controller->wire.command,
                          controller->wire.payload[0]);
    if (result == H9_WIRE_FRAME) {
        dispatch(controller);
        h9_wire_reset(&controller->wire);
    }
}

bool h9_aprs_controller_forward_aux(H9AprsController *controller,
                                     const uint8_t *bytes, size_t length)
{
    if (controller == NULL || bytes == NULL || length == 0 ||
        length > H9_APRS_MAX_SEND) return false;
    return send_frame(controller, 0x10u, bytes, length);
}

bool h9_aprs_controller_pop_received_byte(H9AprsController *controller,
                                          uint8_t *byte)
{
    return controller != NULL && h9_aprs_demod_pop_byte(&controller->demod, byte);
}

void h9_aprs_controller_process_received(H9AprsController *controller)
{
    uint8_t byte;
    uint32_t overflow_count;
    if (controller == NULL) return;
    overflow_count = atomic_load_explicit(&controller->demod.queue_overflow,
                                           memory_order_acquire);
    if (overflow_count != controller->receive_overflow_seen) {
        controller->receive_overflow_seen = overflow_count;
        h9_aprs_ring_clear_consumer(&controller->demod.receive);
        h9_aprs_frame_init(&controller->frame);
    }
    while (h9_aprs_demod_pop_byte(&controller->demod, &byte))
        if (h9_aprs_frame_feed(&controller->frame, byte,
                               forward_valid_frame, controller))
            ++controller->valid_rx_frames;
}

bool h9_aprs_controller_report_burst_activity(H9AprsController *controller,
                                               uint32_t now_ms)
{
    H9AprsDemodGateState gate;
    bool active;
    uint8_t payload[2];
    if (controller == NULL) return false;
    gate = h9_aprs_demod_gate_state(&controller->demod);
    active = gate.in_frame || gate.preamble_seen;
    if ((!controller->burst_report_valid && !active) ||
        (controller->burst_report_valid &&
         controller->burst_reported_active == active &&
         (!active ||
          (uint32_t)(now_ms - controller->last_burst_report_ms) <
              H9_APRS_BURST_RENEW_MS)))
        return false;
    payload[0] = H9_APRS_BURST_VERSION;
    payload[1] = active ? H9_APRS_BURST_ACTIVE : 0u;
    if (!send_frame(controller, H9_APRS_BURST_COMMAND,
                    payload, sizeof payload)) return false;
    controller->burst_report_valid = true;
    controller->burst_reported_active = active;
    controller->last_burst_report_ms = now_ms;
    return true;
}

bool h9_aprs_controller_report_tx_encoded(H9AprsController *controller)
{
    static const uint8_t accepted = 1u;
    return controller != NULL && controller->tx_status_enabled &&
           send_frame(controller, 0x0Fu, &accepted, 1u);
}

bool h9_aprs_controller_configure_tx(H9AprsController *controller,
                                     uint16_t lead_ms, uint16_t tail_ms)
{
    if (controller == NULL || controller->tx.active) return false;
    h9_aprs_tx_init(&controller->tx, lead_ms, tail_ms);
    controller->tx_settings.lead_ms = lead_ms;
    controller->tx_settings.tail_ms = tail_ms;
    if (controller->hardware.tx_settings_changed != NULL)
        controller->hardware.tx_settings_changed(controller->hardware.context,
                                                 &controller->tx_settings);
    return true;
}

bool h9_aprs_controller_queue_tx_byte(H9AprsController *controller,
                                      uint8_t byte)
{
    return controller != NULL && h9_aprs_tx_enqueue(&controller->tx, byte);
}

static uint8_t reverse_high_nibble(uint8_t byte)
{
    uint8_t high = (uint8_t)(byte >> 4);
    return (uint8_t)(((high & 1u) << 7) | ((high & 2u) << 5) |
                     ((high & 4u) << 3) | ((high & 8u) << 1));
}

void h9_aprs_controller_adc_tick(H9AprsController *controller)
{
    H9AprsHardware *hw;
    uint8_t scaled;
    int16_t sample;
    uint8_t dac;
    uint8_t wave = 0;
    bool has_wave = false;
    if (controller == NULL) return;
    hw = &controller->hardware;
    if (hw->read_adc == NULL) return;
    scaled = (uint8_t)((hw->read_adc(hw->context) >> 2) - 128u);
    sample = scaled < 128u ? (int16_t)scaled : (int16_t)scaled - 256;
    (void)h9_aprs_demod_sample(&controller->demod, sample);
    if (hw->demod_sample != NULL) hw->demod_sample(hw->context, sample);
    if (hw->read_dac_port != NULL && hw->write_dac_port != NULL) {
        dac = (uint8_t)(hw->read_dac_port(hw->context) & 0x0Fu);
        if (hw->tx_wave_active != NULL && hw->next_tx_wave_sample != NULL) {
            if (hw->tx_wave_active(hw->context))
                has_wave = hw->next_tx_wave_sample(hw->context, &wave);
        } else {
            has_wave = h9_aprs_tx_next_sample(&controller->tx, &wave);
        }
        dac |= has_wave ? reverse_high_nibble(wave) : 0x80u;
        hw->write_dac_port(hw->context, dac);
    }
    ++controller->adc_sample_count;
}
