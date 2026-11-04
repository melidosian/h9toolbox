#include <stddef.h>

#include "h9_aprs_runtime_state.h"

static H9AprsRuntimeState runtime;

/* Temporary freestanding shims for a whole-archive link. The board entry is
   intentionally inert until peripheral integration is source-backed. */
void *memcpy(void *destination, const void *source, size_t length)
{
    unsigned char *to = (unsigned char *)destination;
    const unsigned char *from = (const unsigned char *)source;
    size_t index;
    for (index = 0; index < length; ++index) to[index] = from[index];
    return destination;
}

void *memset(void *destination, int value, size_t length)
{
    unsigned char *to = (unsigned char *)destination;
    size_t index;
    for (index = 0; index < length; ++index) to[index] = (unsigned char)value;
    return destination;
}

__attribute__((noreturn)) void h9_aprs_board_main(void)
{
    h9_aprs_controller_init(&runtime.controller, NULL);
    h9_aprs_tx_service_init(&runtime.tx_service, 0, 0, 0);
    h9_aprs_uart_receive_init(&runtime.uart_receive);
    h9_aprs_uart1_dma_bind_target(&runtime.uart1_dma,
                                  &runtime.uart_receive);
    for (;;) { }
}

void h9_aprs_dma1_channel1_handler(void)
{
    h9_aprs_dma1_channel1_irq(&runtime.uart1_dma);
}

void h9_aprs_usart1_handler(void)
{
    h9_aprs_uart1_irq(&runtime.uart1_dma);
}
