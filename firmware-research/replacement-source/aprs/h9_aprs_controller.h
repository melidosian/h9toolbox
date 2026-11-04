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
    /* Command 0x11/4 writes raw bytes to USART2 when GPIOB bit 5 is high. */
    void (*aux_uart_write)(void *context, const uint8_t *data, size_t length);
    bool (*read_status)(void *context, uint8_t output[48]);
    void (*demod_sample)(void *context, int16_t normalized_sample);
    bool (*tx_wave_active)(void *context);
    bool (*next_tx_wave_sample)(void *context, uint8_t *sample);
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
    bool tx_status_enabled;
} H9AprsController;

void h9_aprs_controller_init(H9AprsController *controller,
                             const H9AprsHardware *hardware);
void h9_aprs_controller_uart_byte(H9AprsController *controller, uint8_t byte);
/* Forward raw USART2 bytes to the main radio as serial command 0x10. */
bool h9_aprs_controller_forward_aux(H9AprsController *controller,
                                     const uint8_t *bytes, size_t length);
bool h9_aprs_controller_pop_received_byte(H9AprsController *controller,
                                          uint8_t *byte);
void h9_aprs_controller_process_received(H9AprsController *controller);
/* Stock command 0x0F requests a one-byte status after a TX frame is encoded
 * into the waveform queue. It does not indicate that RF has finished. */
bool h9_aprs_controller_report_tx_encoded(H9AprsController *controller);
bool h9_aprs_controller_configure_tx(H9AprsController *controller,
                                     uint16_t lead_ms, uint16_t tail_ms);
bool h9_aprs_controller_queue_tx_byte(H9AprsController *controller,
                                      uint8_t byte);
/* This preserves the stock order: demodulation before optional TX waveform. */
void h9_aprs_controller_adc_tick(H9AprsController *controller);

#endif
