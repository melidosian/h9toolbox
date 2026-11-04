#ifndef H9_APRS_UART_RECEIVE_H
#define H9_APRS_UART_RECEIVE_H

#include "h9_aprs_controller.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define H9_APRS_UART_BATCH_CAPACITY 512u

/* Stock 0x08005F20/0x08005F98 uses two 0x200-byte receive buffers. */
typedef struct {
    uint8_t buffers[2][H9_APRS_UART_BATCH_CAPACITY];
    uint16_t pending_length;
    uint8_t current;
    uint8_t ready;
    uint8_t processing;
} H9AprsUartReceive;

typedef void (*H9AprsUartRearm)(void *context, uint8_t *buffer,
                                size_t capacity);

void h9_aprs_uart_receive_init(H9AprsUartReceive *receive);
uint8_t *h9_aprs_uart_receive_buffer(H9AprsUartReceive *receive);
/* The board calls this after DMA has filled the current buffer. */
bool h9_aprs_uart_receive_complete(H9AprsUartReceive *receive,
                                   size_t length);
/* Main-loop half of stock 0x08005F98: feed the pending batch, alternate
 * buffers, and request the next DMA receive. Returns consumed byte count. */
size_t h9_aprs_uart_receive_process(H9AprsUartReceive *receive,
                                    H9AprsController *controller,
                                    H9AprsUartRearm rearm, void *context);

#endif
