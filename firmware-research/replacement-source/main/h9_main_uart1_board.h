#ifndef H9_MAIN_UART1_BOARD_H
#define H9_MAIN_UART1_BOARD_H

#include <stdint.h>

#include "h9_main_aprs_uart1_link.h"
#include "h9_main_serial_profile.h"

/* The stock UART1 open path calls ROM DMA, interrupt, pin, and clock helpers.
 * Their implementations remain target-specific; do not bind this to hardware
 * until the helper addresses and calling conventions have been verified. */
typedef struct {
    void (*configure_pins)(void *context, uint8_t tx_pin, uint8_t rx_pin);
    void (*initialize_dma)(void *context, uint32_t descriptor_address);
    void (*register_irq)(void *context, uint8_t interrupt_number,
                         uint8_t priority, void (*handler)(void *),
                         void *handler_context);
    void (*configure_baud)(void *context, uint32_t half_baud);
    void *context;
} H9MainUart1BoardHooks;

/* Port of the UART1 branch of stock app+0x0C57BC. The link and record must
 * describe the same RX DMA storage. Returns zero before changing hardware if
 * any required resource/hook is absent. Does not select a fallback UART. */
int h9_main_uart1_board_open(H9MainAprsUart1Link *link,
                              const H9MainSerialOpenRecord *record,
                              const H9MainUart1BoardHooks *hooks);

#endif
