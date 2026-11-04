#include "h9_aprs_controller.h"
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
    (void)send_frame(controller, 0x00u, data, length);
}

static void send_version(H9AprsController *controller, uint8_t query)
{
    static const uint8_t module_version[8] = "V1.0.15";
    static const uint8_t protocol_version[7] = "V1.0.0";
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
    case 0x0Fu:
        if (length != 0u)
            controller->tx_status_enabled = data[length - 1u] != 0u;
        break;
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
    default:
        break;
    }
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
    h9_aprs_tx_init(&controller->tx, 0, 0);
}

void h9_aprs_controller_uart_byte(H9AprsController *controller, uint8_t byte)
{
    H9WireResult result;
    if (controller == NULL) return;
    result = h9_wire_feed(&controller->wire, byte);
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
    if (controller == NULL) return;
    while (h9_aprs_demod_pop_byte(&controller->demod, &byte))
        (void)h9_aprs_frame_feed(&controller->frame, byte,
                                 forward_valid_frame, controller);
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
    if (hw->read_dac_port == NULL || hw->write_dac_port == NULL) return;
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
