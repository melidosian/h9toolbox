#ifndef H9_APRS_CONTROLLER_H
#define H9_APRS_CONTROLLER_H

#include "h9_aprs_wire.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Hardware-facing operations still require a board-specific implementation. */
typedef struct {
    void (*uart_write)(void *context, const uint8_t *data, size_t length);
    void (*set_radio_gpio)(void *context, bool high);
    bool (*read_radio_gpio)(void *context);
    void (*queue_packet_tx)(void *context, const uint8_t *packet, size_t length);
    bool (*read_status)(void *context, uint8_t output[48]);
    void (*demod_sample)(void *context, int16_t normalized_sample);
    bool (*tx_wave_active)(void *context);
    uint8_t (*next_tx_wave_sample)(void *context);
    uint16_t (*read_adc)(void *context);
    uint8_t (*read_dac_port)(void *context);
    void (*write_dac_port)(void *context, uint8_t value);
    void *context;
} H9AprsHardware;

typedef struct {
    H9WireDecoder wire;
    H9AprsHardware hardware;
    uint8_t tx_frame[H9_APRS_ENCODED_CAPACITY];
} H9AprsController;

void h9_aprs_controller_init(H9AprsController *controller,
                             const H9AprsHardware *hardware);
void h9_aprs_controller_uart_byte(H9AprsController *controller, uint8_t byte);
bool h9_aprs_controller_received_packet(H9AprsController *controller,
                                        const uint8_t *packet, size_t length);
/* This preserves the stock order: demodulation before optional TX waveform. */
void h9_aprs_controller_adc_tick(H9AprsController *controller);

#endif
