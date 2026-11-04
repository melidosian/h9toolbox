#include "h9_aprs_uart1_board.h"

#include <stddef.h>

#define H9_APRS_UART1_BAUD 115200u

/* These offsets are taken from APRS 0x08004484, 0x08004CE4, 0x08004A50. */
enum {
    RCC_AHBENR = 0x38 / 4, RCC_GPIOENR = 0x34 / 4,
    RCC_USARTENR = 0x40 / 4, RCC_PRESCALER = 0x08 / 4
};
enum { GPIO_MODER, GPIO_OTYPER, GPIO_OSPEEDR, GPIO_PUPDR,
       GPIO_AFRL = 0x20 / 4 };
enum { USART_SR, USART_DR, USART_BRR, USART_CR1, USART_CR2, USART_CR3 };

void h9_aprs_uart1_board_bind(H9AprsUart1Board *board,
                              volatile uint32_t *rcc,
                              volatile uint32_t *gpio,
                              volatile uint32_t *usart,
                              volatile uint32_t *nvic_iser)
{
    if (board == NULL) return;
    board->rcc = rcc;
    board->gpio = gpio;
    board->usart = usart;
    board->nvic_iser = nvic_iser;
}

void h9_aprs_uart1_board_bind_target(H9AprsUart1Board *board)
{
    h9_aprs_uart1_board_bind(board,
        (volatile uint32_t *)(uintptr_t)0x40021000u,
        (volatile uint32_t *)(uintptr_t)0x50000400u,
        (volatile uint32_t *)(uintptr_t)0x40013800u,
        (volatile uint32_t *)(uintptr_t)0xE000E100u);
}

uint32_t h9_aprs_uart1_peripheral_clock(uint32_t system_clock_hz,
                                        uint32_t rcc_prescaler)
{
    static const uint8_t shift[8] = { 0, 0, 0, 0, 1, 2, 3, 4 };
    return system_clock_hz >> shift[(rcc_prescaler >> 12) & 7u];
}

uint32_t h9_aprs_uart_brr(uint32_t peripheral_clock_hz, uint32_t baud)
{
    /* Stock 0x08004CE4 uses an x25 / (baud x4) intermediate, then
     * separates its decimal mantissa and fractional part. */
    uint64_t scaled;
    uint32_t mantissa;
    uint32_t fraction;
    if (baud == 0u || peripheral_clock_hz / 16u < baud) return 0;
    scaled = (uint64_t)peripheral_clock_hz * 25u /
             ((uint64_t)baud * 4u);
    mantissa = (uint32_t)(scaled / 100u);
    fraction = (uint32_t)(((scaled % 100u) * 16u + 50u) / 100u);
    if (fraction == 16u) {
        ++mantissa;
        fraction = 0;
    }
    if (mantissa > 0xFFFu) return 0;
    return (mantissa << 4) | fraction;
}

uint32_t h9_aprs_uart1_brr(uint32_t peripheral_clock_hz)
{
    return h9_aprs_uart_brr(peripheral_clock_hz, H9_APRS_UART1_BAUD);
}

bool h9_aprs_uart1_board_configure(H9AprsUart1Board *board,
                                   uint32_t system_clock_hz)
{
    uint32_t peripheral_hz;
    uint32_t brr;
    unsigned pin;
    if (board == NULL || board->rcc == NULL || board->gpio == NULL ||
        board->usart == NULL || board->nvic_iser == NULL ||
        system_clock_hz == 0) return false;
    peripheral_hz = h9_aprs_uart1_peripheral_clock(
        system_clock_hz, board->rcc[RCC_PRESCALER]);
    brr = h9_aprs_uart1_brr(peripheral_hz);
    if (brr == 0) return false;

    board->rcc[RCC_AHBENR] |= 1u;
    board->rcc[RCC_GPIOENR] |= 2u;
    board->rcc[RCC_USARTENR] |= 0x4000u;
    for (pin = 6; pin <= 7; ++pin) {
        const uint32_t pair_shift = pin * 2u;
        const uint32_t af_shift = pin * 4u;
        board->gpio[GPIO_MODER] =
            (board->gpio[GPIO_MODER] & ~(3u << pair_shift)) |
            (2u << pair_shift);
        board->gpio[GPIO_OTYPER] &= ~(1u << pin);
        board->gpio[GPIO_OSPEEDR] =
            (board->gpio[GPIO_OSPEEDR] & ~(3u << pair_shift)) |
            (3u << pair_shift);
        board->gpio[GPIO_PUPDR] =
            (board->gpio[GPIO_PUPDR] & ~(3u << pair_shift)) |
            (1u << pair_shift);
        board->gpio[GPIO_AFRL] &= ~(0xFu << af_shift);
    }
    board->usart[USART_CR1] &= ~0x2000u;
    board->usart[USART_CR1] =
        (board->usart[USART_CR1] & 0xFFFFE9F3u) | 0xCu;
    board->usart[USART_CR2] &= ~0x2800u;
    board->usart[USART_CR3] &= ~0xB08u;
    board->usart[USART_BRR] = brr;
    board->usart[USART_CR1] |= 0x2000u;
    return true;
}

void h9_aprs_uart1_board_enable_irqs(H9AprsUart1Board *board)
{
    if (board == NULL || board->nvic_iser == NULL) return;
    /* The stock updater sets both priorities to zero, then enables them. */
    *board->nvic_iser = (1u << 9) | (1u << 27);
}
