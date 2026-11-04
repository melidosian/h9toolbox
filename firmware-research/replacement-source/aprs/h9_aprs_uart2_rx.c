#include "h9_aprs_uart2_rx.h"

#include <stddef.h>

enum { USART_SR, USART_DR, USART_BRR, USART_CR1 };
enum {
    USART_ORE = 0x08u,
    USART_IDLE = 0x10u,
    USART_RXNE = 0x20u,
    USART_IDLEIE = 0x10u,
    USART_RXNEIE = 0x20u,
    USART2_IRQ = 28u
};

void h9_aprs_uart2_rx_bind(H9AprsUart2Rx *receiver,
                            volatile uint32_t *usart,
                            volatile uint32_t *nvic_iser)
{
    if (receiver == NULL) return;
    receiver->usart = usart;
    receiver->nvic_iser = nvic_iser;
    receiver->length = 0u;
    receiver->ready = false;
    receiver->overrun_count = 0u;
    receiver->dropped_count = 0u;
}

void h9_aprs_uart2_rx_bind_target(H9AprsUart2Rx *receiver)
{
    h9_aprs_uart2_rx_bind(receiver,
        (volatile uint32_t *)(uintptr_t)0x40004400u,
        (volatile uint32_t *)(uintptr_t)0xE000E100u);
}

bool h9_aprs_uart2_rx_start(H9AprsUart2Rx *receiver)
{
    if (receiver == NULL || receiver->usart == NULL ||
        receiver->nvic_iser == NULL) return false;
    receiver->length = 0u;
    receiver->ready = false;
    receiver->usart[USART_CR1] |= USART_RXNEIE | USART_IDLEIE;
    *receiver->nvic_iser = 1u << USART2_IRQ;
    return true;
}

void h9_aprs_uart2_rx_irq(H9AprsUart2Rx *receiver)
{
    uint32_t status;
    uint32_t control;
    uint8_t byte;
    if (receiver == NULL || receiver->usart == NULL) return;
    status = receiver->usart[USART_SR];
    control = receiver->usart[USART_CR1];
    if ((status & (USART_ORE | USART_IDLE | USART_RXNE)) == 0u) return;
    /* Reading SR then DR also clears IDLE and overrun on this USART. */
    byte = (uint8_t)receiver->usart[USART_DR];
    if ((status & USART_ORE) != 0u) {
        ++receiver->overrun_count;
        receiver->length = 0u;
    } else if ((status & USART_RXNE) != 0u &&
               (control & USART_RXNEIE) != 0u) {
        if (receiver->ready) {
            ++receiver->dropped_count;
        } else if (receiver->length < sizeof receiver->bytes) {
            receiver->bytes[receiver->length++] = byte;
        } else {
            ++receiver->dropped_count;
        }
    }
    if ((!receiver->ready && receiver->length == sizeof receiver->bytes) ||
        ((status & USART_IDLE) != 0u &&
         (control & USART_IDLEIE) != 0u && receiver->length != 0u)) {
        receiver->ready = true;
        receiver->usart[USART_CR1] &= ~(USART_RXNEIE | USART_IDLEIE);
    }
}

bool h9_aprs_uart2_rx_poll(H9AprsUart2Rx *receiver,
                            H9AprsController *controller)
{
    bool sent;
    size_t forward_length;
    if (receiver == NULL || receiver->usart == NULL ||
        controller == NULL || !receiver->ready) return false;
    forward_length = receiver->length;
    if (forward_length > H9_APRS_MAX_SEND) {
        receiver->dropped_count += (uint32_t)(forward_length -
                                               H9_APRS_MAX_SEND);
        forward_length = H9_APRS_MAX_SEND;
    }
    sent = h9_aprs_controller_forward_aux(controller, receiver->bytes,
                                          forward_length);
    if (!sent) receiver->dropped_count += (uint32_t)forward_length;
    receiver->length = 0u;
    receiver->ready = false;
    receiver->usart[USART_CR1] |= USART_RXNEIE | USART_IDLEIE;
    return sent;
}
