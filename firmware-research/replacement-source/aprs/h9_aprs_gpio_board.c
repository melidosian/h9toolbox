#include "h9_aprs_gpio_board.h"

#include <stddef.h>

#define H9_APRS_GPIO_B_BASE 0x50000400u
#define H9_APRS_GPIO_BIT_5 0x20u
#define H9_APRS_GPIO_IDR_WORD (0x10u / 4u)
#define H9_APRS_GPIO_SET_WORD (0x18u / 4u)
#define H9_APRS_GPIO_CLEAR_WORD (0x28u / 4u)

void h9_aprs_gpio_board_bind(H9AprsGpioBoard *board,
                             volatile uint32_t *gpio_b)
{
    if (board != NULL) board->gpio_b = gpio_b;
}

void h9_aprs_gpio_board_bind_target(H9AprsGpioBoard *board)
{
    h9_aprs_gpio_board_bind(board,
        (volatile uint32_t *)(uintptr_t)H9_APRS_GPIO_B_BASE);
}

void h9_aprs_gpio_board_write(H9AprsGpioBoard *board, bool high)
{
    if (board == NULL || board->gpio_b == NULL) return;
    board->gpio_b[high ? H9_APRS_GPIO_SET_WORD : H9_APRS_GPIO_CLEAR_WORD] =
        H9_APRS_GPIO_BIT_5;
}

bool h9_aprs_gpio_board_read(const H9AprsGpioBoard *board)
{
    return board != NULL && board->gpio_b != NULL &&
           (board->gpio_b[H9_APRS_GPIO_IDR_WORD] & H9_APRS_GPIO_BIT_5) != 0u;
}
