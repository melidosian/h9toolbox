#ifndef H9_MAIN_UART1_BOARD_H
#define H9_MAIN_UART1_BOARD_H

#include <stdbool.h>
#include <stdint.h>

#include "h9_main_aprs_uart1_link.h"
#include "h9_main_serial_profile.h"

/* The stock UART1 open path calls ROM DMA, interrupt, pin, and clock helpers.
 * The target adapter maps observed entry points; radio behavior is untested. */
typedef struct {
    bool (*configure_pins)(void *context, uint8_t tx_pin, uint8_t rx_pin);
    bool (*initialize_dma)(void *context, uint32_t descriptor_address);
    bool (*register_irq)(void *context, uint8_t interrupt_number,
                         uint8_t priority, void (*handler)(void *),
                         void *handler_context);
    bool (*configure_baud)(void *context, uint32_t half_baud,
                           uint32_t field_10);
    void *context;
} H9MainUart1BoardHooks;

/* Port of the UART1 branch of stock app+0x0C57BC. The link and record must
 * describe the same RX DMA storage. Returns zero before changing hardware if
 * any required resource/hook is absent, and stops if a hook fails. Does not
 * select a fallback UART. A failed later hook may leave partial MMIO writes. */
int h9_main_uart1_board_open(H9MainAprsUart1Link *link,
                              const H9MainSerialOpenRecord *record,
                              const H9MainUart1BoardHooks *hooks);

#endif
