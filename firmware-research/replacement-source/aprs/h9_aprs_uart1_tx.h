#ifndef H9_APRS_UART1_TX_H
#define H9_APRS_UART1_TX_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* Stock 0x08004870/0x0800476C sends each serial byte through USART1 DR,
 * waiting for SR.TXE (0x80) and SR.TC (0x40). Our poll bound replaces the
 * stock millisecond timer until that timebase is ported. */
typedef struct {
    volatile uint32_t *usart;
    uint32_t poll_limit;
    uint32_t bytes_completed;
    uint32_t timeouts;
    bool faulted;
} H9AprsUart1Tx;

void h9_aprs_uart1_tx_bind(H9AprsUart1Tx *tx,
                           volatile uint32_t *usart,
                           uint32_t poll_limit);
void h9_aprs_uart1_tx_bind_target(H9AprsUart1Tx *tx,
                                  uint32_t poll_limit);
/* Returns the number of bytes whose transmit-complete flag was observed. */
size_t h9_aprs_uart1_tx_write(H9AprsUart1Tx *tx,
                              const uint8_t *data, size_t length);
void h9_aprs_uart1_tx_callback(void *context,
                               const uint8_t *data, size_t length);
bool h9_aprs_uart1_tx_ok(void *context);

#endif
