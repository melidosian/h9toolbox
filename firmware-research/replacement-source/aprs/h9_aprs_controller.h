#ifndef H9_APRS_CONTROLLER_H
#define H9_APRS_CONTROLLER_H

#include "h9_aprs_wire.h"
#include "h9_aprs_demod.h"
#include "h9_aprs_frame.h"
#include "h9_aprs_tx.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Hardware-facing operations still require a board-specific implementation. */
typedef struct {
    /* Each outgoing fragment is temporary: transmit or copy it before return. */
    void (*uart_write)(void *context, const uint8_t *data, size_t length);
    bool (*uart_ok)(void *context);
    void (*set_radio_gpio)(void *context, bool high);
    bool (*read_radio_gpio)(void *context);
    /* Called synchronously for serial command 0x00; copy packet if deferred. */
    void (*transmit_aprs_packet)(void *context, const uint8_t *packet,
                                 size_t length);
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
    H9AprsDemod demod;
    H9AprsFrame frame;
    H9AprsTx tx;
    H9AprsHardware hardware;
} H9AprsController;

void h9_aprs_controller_init(H9AprsController *controller,
                             const H9AprsHardware *hardware);
void h9_aprs_controller_uart_byte(H9AprsController *controller, uint8_t byte);
bool h9_aprs_controller_received_packet(H9AprsController *controller,
                                        const uint8_t *packet, size_t length);
bool h9_aprs_controller_pop_received_byte(H9AprsController *controller,
                                          uint8_t *byte);
void h9_aprs_controller_process_received(H9AprsController *controller);
bool h9_aprs_controller_configure_tx(H9AprsController *controller,
                                     uint16_t lead_ms, uint16_t tail_ms);
bool h9_aprs_controller_queue_tx_byte(H9AprsController *controller,
                                      uint8_t byte);
/* This preserves the stock order: demodulation before optional TX waveform. */
void h9_aprs_controller_adc_tick(H9AprsController *controller);

#endif
