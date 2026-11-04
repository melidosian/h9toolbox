#include "h9_aprs_uart_receive.h"

#include <string.h>

void h9_aprs_uart_receive_init(H9AprsUartReceive *receive)
{
    if (receive != NULL) memset(receive, 0, sizeof *receive);
}

uint8_t *h9_aprs_uart_receive_buffer(H9AprsUartReceive *receive)
{
    if (receive == NULL || receive->ready || receive->processing ||
        receive->current > 1u) return NULL;
    return receive->buffers[receive->current];
}

bool h9_aprs_uart_receive_complete(H9AprsUartReceive *receive,
                                   size_t length)
{
    if (receive == NULL || receive->ready || receive->processing ||
        receive->current > 1u || length == 0 ||
        length > H9_APRS_UART_BATCH_CAPACITY) return false;
    receive->pending_length = (uint16_t)length;
    receive->ready = 1;
    return true;
}

size_t h9_aprs_uart_receive_process(H9AprsUartReceive *receive,
                                    H9AprsController *controller,
                                    H9AprsUartRearm rearm, void *context)
{
    size_t length;
    size_t index;
    if (receive == NULL || controller == NULL || !receive->ready ||
        receive->processing || receive->current > 1u ||
        receive->pending_length == 0 ||
        receive->pending_length > H9_APRS_UART_BATCH_CAPACITY) return 0;
    receive->processing = 1;
    length = receive->pending_length;
    for (index = 0; index < length; ++index)
        h9_aprs_controller_uart_byte(controller,
                                      receive->buffers[receive->current][index]);
    receive->current ^= 1u;
    receive->processing = 0;
    receive->ready = 0;
    receive->pending_length = 0;
    if (rearm != NULL)
        rearm(context, receive->buffers[receive->current],
              H9_APRS_UART_BATCH_CAPACITY);
    return length;
}
