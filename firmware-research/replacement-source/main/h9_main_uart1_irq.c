#include "h9_main_uart1_irq.h"

#include <stddef.h>

enum {
    UART_CONTROL = 0,
    UART_STATUS = 1,
    UART_RX_PARTIAL_BYTES = 10
};

static void report(H9MainUart1Irq *irq, unsigned number)
{
    if (irq->event != NULL) irq->event(irq->context, number);
}

static void acknowledge_rx(H9MainUart1Irq *irq)
{
    if (irq->acknowledge_rx_dma != NULL)
        irq->acknowledge_rx_dma(irq->context);
}

void h9_main_uart1_irq(H9MainUart1Irq *irq)
{
    volatile uint32_t *uart;
    uint32_t control;
    uint32_t partial;
    if (irq == NULL || irq->registers == NULL || irq->receive == NULL) return;
    uart = irq->registers;

    control = uart[UART_CONTROL];
    if ((control & 0x8004u) == 0x8004u) {
        uart[UART_CONTROL] |= 0x2000u;
        if (irq->acknowledge_tx_dma != NULL)
            irq->acknowledge_tx_dma(irq->context);
        report(irq, 1u);
    }

    control = uart[UART_CONTROL];
    if ((control & 0x4008u) == 0x4008u) {
        uart[UART_CONTROL] |= 0x1480u;
        irq->receive->produced += irq->receive_chunk_bytes;
        if ((uart[UART_STATUS] & 1u) != 0u)
            uart[UART_STATUS] |= 0x2000u;
        acknowledge_rx(irq);
        report(irq, 2u);
    }

    control = uart[UART_CONTROL];
    if ((control & 0x0820u) == 0x0820u) {
        uart[UART_CONTROL] |= 0x1480u;
        partial = uart[UART_RX_PARTIAL_BYTES];
        if ((uart[UART_STATUS] & 1u) != 0u)
            uart[UART_STATUS] |= 0x2000u;
        if (partial != 0u) {
            irq->receive->produced += partial;
            acknowledge_rx(irq);
            report(irq, 3u);
        }
    }
}
