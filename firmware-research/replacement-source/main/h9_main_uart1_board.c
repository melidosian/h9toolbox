#include "h9_main_uart1_board.h"

#include <stddef.h>
#include <stdint.h>

enum {
    UART_CONTROL = 0,
    UART_RX_END = 8,
    UART_RX_BASE = 7,
    UART_RX_THRESHOLD = 9
};

static void uart1_vector(void *context)
{
    h9_main_aprs_uart1_link_irq((H9MainAprsUart1Link *)context);
}

int h9_main_uart1_board_open(H9MainAprsUart1Link *link,
                              const H9MainSerialOpenRecord *record,
                              const H9MainUart1BoardHooks *hooks)
{
    volatile uint32_t *uart;
    if (link == NULL || record == NULL || hooks == NULL ||
        link->irq.registers == NULL || link->irq.receive != &link->receive ||
        link->receive.storage == NULL ||
        record->tx_pin != 0x20u || record->rx_pin != 0x21u ||
        record->storage_address == 0u ||
        record->storage_size != link->receive.capacity ||
        record->read_limit == 0u ||
        record->read_limit != link->irq.receive_chunk_bytes ||
        record->read_limit > record->storage_size ||
        record->storage_address > UINT32_MAX - record->storage_size ||
        record->baud < 2u ||
        hooks->configure_pins == NULL || hooks->initialize_dma == NULL ||
        hooks->register_irq == NULL || hooks->configure_baud == NULL)
        return 0;

    uart = link->irq.registers;
    if ((uart[UART_CONTROL] & 1u) != 0u) return 0;
    if (!hooks->configure_pins(hooks->context, record->tx_pin,
                               record->rx_pin)) return 0;
    uart[UART_CONTROL] = 0x3400u;
    if (!hooks->initialize_dma(hooks->context, 0x001E360u) ||
        !hooks->initialize_dma(hooks->context, 0x001E3B0u)) return 0;
    if (!hooks->register_irq(hooks->context, 0x13u, 3u,
                             uart1_vector, link)) return 0;
    link->receive.produced = 0u;
    link->receive.consumed = 0u;
    uart[UART_RX_END] = record->storage_address + record->storage_size;
    uart[UART_CONTROL] |= 0x68u;
    uart[UART_RX_BASE] = record->storage_address;
    uart[UART_RX_THRESHOLD] = record->read_limit;
    if ((record->baud & 1u) != 0u)
        uart[UART_CONTROL] |= 2u;
    else
        uart[UART_CONTROL] &= ~2u;
    return hooks->configure_baud(hooks->context,
                                 (record->baud & 0x1FFFFFEu) >> 1);
}
