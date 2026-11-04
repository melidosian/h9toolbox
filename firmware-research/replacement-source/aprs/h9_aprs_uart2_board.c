#include "h9_aprs_uart2_board.h"
#include "h9_aprs_uart1_board.h"

#include <stddef.h>

enum { RCC_GPIOENR = 0x34 / 4, RCC_USARTENR = 0x40 / 4,
       RCC_PRESCALER = 0x08 / 4 };
enum { GPIO_MODER, GPIO_OTYPER, GPIO_OSPEEDR, GPIO_PUPDR,
       GPIO_AFRL = 0x20 / 4 };
enum { USART_SR, USART_DR, USART_BRR, USART_CR1, USART_CR2, USART_CR3 };

void h9_aprs_uart2_board_bind(H9AprsUart2Board *board,
                               volatile uint32_t *rcc,
                               volatile uint32_t *gpio_c,
                               volatile uint32_t *usart,
                               uint32_t poll_limit)
{
    if (board == NULL) return;
    board->rcc = rcc;
    board->gpio_c = gpio_c;
    board->usart = usart;
    board->poll_limit = poll_limit;
    board->bytes_completed = 0;
    board->timeouts = 0;
    board->faulted = false;
}

void h9_aprs_uart2_board_bind_target(H9AprsUart2Board *board,
                                      uint32_t poll_limit)
{
    h9_aprs_uart2_board_bind(board,
        (volatile uint32_t *)(uintptr_t)0x40021000u,
        (volatile uint32_t *)(uintptr_t)0x50001400u,
        (volatile uint32_t *)(uintptr_t)0x40004400u,
        poll_limit);
}

bool h9_aprs_uart2_board_configure(H9AprsUart2Board *board,
                                    uint32_t system_clock_hz)
{
    uint32_t peripheral_hz;
    uint32_t brr;
    unsigned pin;
    if (board == NULL || board->rcc == NULL || board->gpio_c == NULL ||
        board->usart == NULL || system_clock_hz == 0u) return false;
    peripheral_hz = h9_aprs_uart1_peripheral_clock(
        system_clock_hz, board->rcc[RCC_PRESCALER]);
    brr = h9_aprs_uart_brr(peripheral_hz, 9600u);
    if (brr == 0u) return false;

    board->rcc[RCC_GPIOENR] |= 0x20u;
    board->rcc[RCC_USARTENR] |= 0x20000u;
    for (pin = 0; pin <= 1; ++pin) {
        uint32_t pair_shift = pin * 2u;
        uint32_t af_shift = pin * 4u;
        board->gpio_c[GPIO_MODER] =
            (board->gpio_c[GPIO_MODER] & ~(3u << pair_shift)) |
            (2u << pair_shift);
        board->gpio_c[GPIO_OTYPER] &= ~(1u << pin);
        board->gpio_c[GPIO_OSPEEDR] =
            (board->gpio_c[GPIO_OSPEEDR] & ~(3u << pair_shift)) |
            (2u << pair_shift);
        board->gpio_c[GPIO_PUPDR] =
            (board->gpio_c[GPIO_PUPDR] & ~(3u << pair_shift)) |
            (1u << pair_shift);
        board->gpio_c[GPIO_AFRL] =
            (board->gpio_c[GPIO_AFRL] & ~(0xFu << af_shift)) |
            (4u << af_shift);
    }
    board->usart[USART_CR1] &= ~0x2000u;
    board->usart[USART_CR1] =
        (board->usart[USART_CR1] & 0xFFFFE9F3u) | 0xCu;
    board->usart[USART_CR2] &= ~0x2800u;
    board->usart[USART_CR3] &= ~0xB08u;
    board->usart[USART_BRR] = brr;
    board->usart[USART_CR1] |= 0x2000u;
    board->faulted = false;
    return true;
}

bool h9_aprs_uart2_board_recover(H9AprsUart2Board *board,
                                  H9AprsUart2Dma *receive_dma,
                                  uint32_t system_clock_hz)
{
    uint32_t peripheral_hz;
    if (board == NULL || receive_dma == NULL || !board->faulted ||
        board->rcc == NULL || board->usart == NULL ||
        board->gpio_c == NULL || receive_dma->rcc != board->rcc ||
        receive_dma->usart != board->usart ||
        receive_dma->receive == NULL || receive_dma->receive->ready ||
        receive_dma->spill_active || system_clock_hz == 0u)
        return false;
    peripheral_hz = h9_aprs_uart1_peripheral_clock(
        system_clock_hz, board->rcc[RCC_PRESCALER]);
    if (h9_aprs_uart_brr(peripheral_hz, 9600u) == 0u) return false;
    if (!h9_aprs_uart2_dma_stop(receive_dma) ||
        !h9_aprs_uart2_board_configure(board, system_clock_hz))
        return false;
    if (!h9_aprs_uart2_dma_start(receive_dma)) {
        board->faulted = true;
        return false;
    }
    return true;
}

static bool wait_flag(const H9AprsUart2Board *board, uint32_t flag)
{
    uint32_t poll;
    for (poll = 0; poll < board->poll_limit; ++poll)
        if ((board->usart[USART_SR] & flag) != 0u) return true;
    return false;
}

size_t h9_aprs_uart2_board_write(H9AprsUart2Board *board,
                                  const uint8_t *data, size_t length)
{
    size_t index;
    if (board == NULL || board->usart == NULL || board->poll_limit == 0u ||
        board->faulted || (data == NULL && length != 0u)) return 0;
    for (index = 0; index < length; ++index) {
        if (!wait_flag(board, 0x80u)) {
            ++board->timeouts;
            board->faulted = true;
            return index;
        }
        board->usart[USART_DR] = data[index];
        if (!wait_flag(board, 0x40u)) {
            ++board->timeouts;
            board->faulted = true;
            return index;
        }
        ++board->bytes_completed;
    }
    return length;
}
