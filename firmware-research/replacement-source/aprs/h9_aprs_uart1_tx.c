#include "h9_aprs_uart1_tx.h"

#include <stddef.h>

enum { USART_SR, USART_DR };
#define USART_TXE 0x80u
#define USART_TC 0x40u

void h9_aprs_uart1_tx_bind(H9AprsUart1Tx *tx,
                           volatile uint32_t *usart,
                           uint32_t poll_limit)
{
    if (tx == NULL) return;
    tx->usart = usart;
    tx->poll_limit = poll_limit;
    tx->bytes_completed = 0;
    tx->timeouts = 0;
    tx->faulted = false;
}

void h9_aprs_uart1_tx_bind_target(H9AprsUart1Tx *tx,
                                  uint32_t poll_limit)
{
    h9_aprs_uart1_tx_bind(tx,
        (volatile uint32_t *)(uintptr_t)0x40013800u, poll_limit);
}

static int wait_flag(const H9AprsUart1Tx *tx, uint32_t flag)
{
    uint32_t poll;
    for (poll = 0; poll < tx->poll_limit; ++poll)
        if ((tx->usart[USART_SR] & flag) != 0u) return 1;
    return 0;
}

size_t h9_aprs_uart1_tx_write(H9AprsUart1Tx *tx,
                              const uint8_t *data, size_t length)
{
    size_t index;
    if (tx == NULL || tx->usart == NULL || tx->poll_limit == 0u ||
        tx->faulted || (data == NULL && length != 0u)) return 0;
    for (index = 0; index < length; ++index) {
        if (!wait_flag(tx, USART_TXE)) {
            ++tx->timeouts;
            tx->faulted = true;
            return index;
        }
        tx->usart[USART_DR] = data[index];
        if (!wait_flag(tx, USART_TC)) {
            ++tx->timeouts;
            tx->faulted = true;
            return index;
        }
        ++tx->bytes_completed;
    }
    return length;
}

void h9_aprs_uart1_tx_callback(void *context,
                               const uint8_t *data, size_t length)
{
    (void)h9_aprs_uart1_tx_write((H9AprsUart1Tx *)context, data, length);
}

bool h9_aprs_uart1_tx_ok(void *context)
{
    H9AprsUart1Tx *tx = (H9AprsUart1Tx *)context;
    return tx != NULL && !tx->faulted;
}
