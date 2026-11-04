#include "h9_aprs_uart2_dma.h"

#include <stddef.h>

enum { USART_SR, USART_DR, USART_BRR, USART_CR1, USART_CR2, USART_CR3 };
enum { DMA_ISR, DMA_IFCR };
enum { DMA_CCR, DMA_CNDTR, DMA_CPAR, DMA_CMAR };

#define DMA_CLOCK_BIT 0x1u
#define DMA_CHANNEL3_FLAGS 0xF00u
#define DMA_CHANNEL3_TC 0x200u
#define DMA_CHANNEL3_TE 0x800u
#define DMA_RX_CCR 0x108Bu
#define DMA_CHANNEL3_REQUEST_MASK (0x1Fu << 16)
#define DMA_CHANNEL3_USART2_REQUEST (8u << 16)
#define USART_IDLE 0x10u
#define USART_IDLEIE 0x10u
#define USART_RXNEIE 0x20u
#define USART_DMAR 0x40u

static bool valid(const H9AprsUart2Dma *driver)
{
    return driver != NULL && driver->usart != NULL &&
           driver->dma_controller != NULL && driver->dma_channel != NULL &&
           driver->dma_request != NULL &&
           driver->rcc != NULL && driver->nvic_iser != NULL &&
           driver->receive != NULL;
}

void h9_aprs_uart2_dma_bind(H9AprsUart2Dma *driver,
                             volatile uint32_t *usart,
                             volatile uint32_t *dma_controller,
                             volatile uint32_t *dma_channel,
                             volatile uint32_t *dma_request,
                             volatile uint32_t *rcc,
                             volatile uint32_t *nvic_iser,
                             H9AprsUart2Rx *receive)
{
    if (driver == NULL) return;
    driver->usart = usart;
    driver->dma_controller = dma_controller;
    driver->dma_channel = dma_channel;
    driver->dma_request = dma_request;
    driver->rcc = rcc;
    driver->nvic_iser = nvic_iser;
    driver->receive = receive;
    driver->transfer_errors = 0u;
    driver->invalid_counts = 0u;
}

void h9_aprs_uart2_dma_bind_target(H9AprsUart2Dma *driver,
                                    H9AprsUart2Rx *receive)
{
    h9_aprs_uart2_dma_bind(driver,
        (volatile uint32_t *)(uintptr_t)0x40004400u,
        (volatile uint32_t *)(uintptr_t)0x40020000u,
        (volatile uint32_t *)(uintptr_t)0x40020030u,
        (volatile uint32_t *)(uintptr_t)0x4001001Cu,
        (volatile uint32_t *)(uintptr_t)0x40021000u,
        (volatile uint32_t *)(uintptr_t)0xE000E100u, receive);
}

static void rearm(H9AprsUart2Dma *driver)
{
    H9AprsUart2Rx *receive = driver->receive;
    driver->dma_channel[DMA_CCR] = DMA_RX_CCR & ~1u;
    driver->dma_controller[DMA_IFCR] = DMA_CHANNEL3_FLAGS;
    driver->dma_channel[DMA_CNDTR] = H9_APRS_UART2_BATCH_CAPACITY;
    driver->dma_channel[DMA_CPAR] =
        (uint32_t)(uintptr_t)&driver->usart[USART_DR];
    driver->dma_channel[DMA_CMAR] = (uint32_t)(uintptr_t)receive->bytes;
    driver->usart[USART_CR1] =
        (driver->usart[USART_CR1] & ~USART_RXNEIE) | USART_IDLEIE;
    driver->usart[USART_CR3] |= USART_DMAR;
    driver->dma_channel[DMA_CCR] = DMA_RX_CCR;
}

bool h9_aprs_uart2_dma_start(H9AprsUart2Dma *driver)
{
    if (!valid(driver)) return false;
    driver->receive->length = 0u;
    driver->receive->ready = false;
    driver->rcc[0x38u / 4u] |= DMA_CLOCK_BIT;
    driver->rcc[0x40u / 4u] |= DMA_CLOCK_BIT;
    *driver->dma_request =
        (*driver->dma_request & ~DMA_CHANNEL3_REQUEST_MASK) |
        DMA_CHANNEL3_USART2_REQUEST;
    rearm(driver);
    /* NVIC set-enable writes are W1S on target. */
    *driver->nvic_iser = (1u << 10) | (1u << 28);
    return true;
}

static void stop_receive(H9AprsUart2Dma *driver)
{
    driver->dma_channel[DMA_CCR] &= ~1u;
    driver->usart[USART_CR3] &= ~USART_DMAR;
    driver->usart[USART_CR1] &= ~USART_IDLEIE;
}

void h9_aprs_uart2_dma_usart_irq(H9AprsUart2Dma *driver)
{
    uint32_t status;
    uint32_t remaining;
    if (!valid(driver) || driver->receive->ready) return;
    status = driver->usart[USART_SR];
    if ((status & USART_IDLE) == 0u ||
        (driver->usart[USART_CR1] & USART_IDLEIE) == 0u) return;
    (void)driver->usart[USART_SR];
    (void)driver->usart[USART_DR];
    stop_receive(driver);
    remaining = driver->dma_channel[DMA_CNDTR];
    if (remaining > H9_APRS_UART2_BATCH_CAPACITY) {
        ++driver->invalid_counts;
        rearm(driver);
        return;
    }
    driver->receive->length =
        (uint16_t)(H9_APRS_UART2_BATCH_CAPACITY - remaining);
    if (driver->receive->length == 0u) {
        rearm(driver);
        return;
    }
    driver->receive->ready = true;
}

void h9_aprs_uart2_dma_channel_irq(H9AprsUart2Dma *driver)
{
    uint32_t flags;
    if (!valid(driver)) return;
    flags = driver->dma_controller[DMA_ISR];
    if ((flags & (DMA_CHANNEL3_TC | DMA_CHANNEL3_TE)) == 0u) return;
    stop_receive(driver);
    driver->dma_controller[DMA_IFCR] = DMA_CHANNEL3_FLAGS;
    if (driver->receive->ready) return;
    if ((flags & DMA_CHANNEL3_TE) != 0u) {
        ++driver->transfer_errors;
        rearm(driver);
        return;
    }
    driver->receive->length = H9_APRS_UART2_BATCH_CAPACITY;
    driver->receive->ready = true;
}

bool h9_aprs_uart2_dma_poll(H9AprsUart2Dma *driver,
                             H9AprsController *controller)
{
    bool sent;
    size_t forward_length;
    if (!valid(driver) || controller == NULL || !driver->receive->ready)
        return false;
    forward_length = driver->receive->length;
    if (forward_length > H9_APRS_MAX_SEND) {
        driver->receive->dropped_count +=
            (uint32_t)(forward_length - H9_APRS_MAX_SEND);
        forward_length = H9_APRS_MAX_SEND;
    }
    sent = h9_aprs_controller_forward_aux(controller,
                                           driver->receive->bytes,
                                           forward_length);
    driver->receive->length = 0u;
    driver->receive->ready = false;
    rearm(driver);
    return sent;
}
