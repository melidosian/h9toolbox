#include "h9_aprs_uart1_dma.h"

#include <stddef.h>

/* USART layout follows the stock register accesses: SR, DR, BRR, CR1,
 * CR2, CR3. DMA channel layout is CCR, CNDTR, CPAR, CMAR. */
enum { USART_SR, USART_DR, USART_BRR, USART_CR1, USART_CR2, USART_CR3 };
enum { DMA_ISR, DMA_IFCR };
enum { DMA_CCR, DMA_CNDTR, DMA_CPAR, DMA_CMAR };

#define USART_IDLE 0x10u
#define USART_IDLEIE 0x10u
#define USART_DMAR 0x40u
#define DMA_CHANNEL1_FLAGS 0x0fu
#define DMA_CHANNEL1_TC 0x02u
#define DMA_CHANNEL1_TE 0x08u
#define DMA_RX_CCR 0x308bu

static bool valid(const H9AprsUart1Dma *driver)
{
    return driver != NULL && driver->usart != NULL &&
           driver->dma_controller != NULL && driver->dma_channel != NULL &&
           driver->receive != NULL;
}

void h9_aprs_uart1_dma_bind(H9AprsUart1Dma *driver,
                            volatile uint32_t *usart,
                            volatile uint32_t *dma_controller,
                            volatile uint32_t *dma_channel,
                            H9AprsUartReceive *receive)
{
    if (driver == NULL) return;
    driver->usart = usart;
    driver->dma_controller = dma_controller;
    driver->dma_channel = dma_channel;
    driver->receive = receive;
    driver->completed_batches = 0u;
    driver->rejected_batches = 0u;
    driver->transfer_errors = 0u;
    driver->last_batch_length = 0u;
}

void h9_aprs_uart1_dma_bind_target(H9AprsUart1Dma *driver,
                                   H9AprsUartReceive *receive)
{
    h9_aprs_uart1_dma_bind(driver,
        (volatile uint32_t *)(uintptr_t)0x40013800u,
        (volatile uint32_t *)(uintptr_t)0x40020000u,
        (volatile uint32_t *)(uintptr_t)0x40020008u, receive);
}

void h9_aprs_uart1_dma_rearm(void *context, uint8_t *buffer, size_t capacity)
{
    H9AprsUart1Dma *driver = (H9AprsUart1Dma *)context;
    if (!valid(driver) || buffer == NULL ||
        capacity != H9_APRS_UART_BATCH_CAPACITY) return;
    driver->dma_channel[DMA_CCR] = 0x3080u;
    driver->dma_controller[DMA_IFCR] = DMA_CHANNEL1_FLAGS;
    driver->dma_channel[DMA_CNDTR] = (uint32_t)capacity;
    driver->dma_channel[DMA_CPAR] =
        (uint32_t)(uintptr_t)&driver->usart[USART_DR];
    driver->dma_channel[DMA_CMAR] = (uint32_t)(uintptr_t)buffer;
    driver->usart[USART_CR1] |= USART_IDLEIE;
    driver->usart[USART_CR3] |= USART_DMAR;
    driver->dma_channel[DMA_CCR] = DMA_RX_CCR;
}

bool h9_aprs_uart1_dma_start(H9AprsUart1Dma *driver)
{
    uint8_t *buffer;
    if (!valid(driver)) return false;
    buffer = h9_aprs_uart_receive_buffer(driver->receive);
    if (buffer == NULL) return false;
    h9_aprs_uart1_dma_rearm(driver, buffer, H9_APRS_UART_BATCH_CAPACITY);
    return true;
}

static void stop_receive(H9AprsUart1Dma *driver)
{
    driver->dma_channel[DMA_CCR] &= ~1u;
    driver->usart[USART_CR3] &= ~USART_DMAR;
}

void h9_aprs_uart1_irq(H9AprsUart1Dma *driver)
{
    uint32_t status;
    uint32_t control;
    size_t remaining;
    if (!valid(driver)) return;
    status = driver->usart[USART_SR];
    control = driver->usart[USART_CR1];
    if ((status & USART_IDLE) == 0u || (control & USART_IDLEIE) == 0u)
        return;
    /* The SR then DR read clears the observed IDLE condition. */
    (void)driver->usart[USART_SR];
    (void)driver->usart[USART_DR];
    stop_receive(driver);
    remaining = driver->dma_channel[DMA_CNDTR];
    if (remaining > H9_APRS_UART_BATCH_CAPACITY) {
        uint8_t *buffer;
        ++driver->rejected_batches;
        buffer = h9_aprs_uart_receive_buffer(driver->receive);
        if (buffer != NULL)
            h9_aprs_uart1_dma_rearm(driver, buffer,
                                    H9_APRS_UART_BATCH_CAPACITY);
        return;
    }
    if (h9_aprs_uart_receive_idle(driver->receive, remaining,
                                  h9_aprs_uart1_dma_rearm, driver)) {
        ++driver->completed_batches;
        driver->last_batch_length =
            (uint16_t)(H9_APRS_UART_BATCH_CAPACITY - remaining);
    } else if (remaining != H9_APRS_UART_BATCH_CAPACITY) {
        ++driver->rejected_batches;
    }
}

void h9_aprs_dma1_channel1_irq(H9AprsUart1Dma *driver)
{
    uint32_t flags;
    if (!valid(driver)) return;
    flags = driver->dma_controller[DMA_ISR];
    if ((flags & (DMA_CHANNEL1_TC | DMA_CHANNEL1_TE)) == 0u) return;
    stop_receive(driver);
    driver->dma_controller[DMA_IFCR] = DMA_CHANNEL1_FLAGS;
    if ((flags & DMA_CHANNEL1_TE) != 0u) {
        uint8_t *buffer = h9_aprs_uart_receive_buffer(driver->receive);
        ++driver->transfer_errors;
        if (buffer != NULL)
            h9_aprs_uart1_dma_rearm(driver, buffer,
                                    H9_APRS_UART_BATCH_CAPACITY);
    } else {
        /* A complete DMA buffer has 512 bytes even without a later IDLE.
         * Stock leaves this case to the UART callback; we publish it here. */
        if (h9_aprs_uart_receive_complete(driver->receive,
                                           H9_APRS_UART_BATCH_CAPACITY)) {
            ++driver->completed_batches;
            driver->last_batch_length = H9_APRS_UART_BATCH_CAPACITY;
        } else {
            ++driver->rejected_batches;
        }
    }
}
