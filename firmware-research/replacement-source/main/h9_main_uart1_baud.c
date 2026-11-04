#include "h9_main_uart1_baud.h"

#include <stddef.h>

void h9_main_uart1_baud_bind_target(H9MainUart1Baud *baud)
{
    if (baud == NULL) return;
    baud->uart_registers = (volatile uint32_t *)(uintptr_t)0x001E2100u;
    baud->clock_ready_d9e4 = (volatile uint32_t *)(uintptr_t)0x0000D9E4u;
    baud->timebase_d9dc = (volatile uint32_t *)(uintptr_t)0x0000D9DCu;
    baud->activity_1e34c = (volatile uint32_t *)(uintptr_t)0x001E34Cu;
}

bool h9_main_uart1_baud_configure(H9MainUart1Baud *baud,
                                  uint32_t half_baud)
{
    volatile uint32_t *uart;
    uint32_t divisor;
    if (baud == NULL || baud->uart_registers == NULL ||
        baud->clock_ready_d9e4 == NULL || baud->timebase_d9dc == NULL ||
        baud->activity_1e34c == NULL || half_baud == 0u ||
        *baud->clock_ready_d9e4 == 0u) return false;

    /* The stock helper uses 48 MHz whenever the D9E4 clock state is nonzero.
     * Its zero-clock branch would underflow the divider; reject that state. */
    divisor = ((48000000u + (half_baud >> 1)) / half_baud >> 2) - 1u;
    uart = baud->uart_registers;
    uart[0] &= ~1u;
    uart[0] |= 0x3400u;
    uart[2] = divisor;
    if ((uart[0] & 0x20u) != 0u) {
        uint32_t activity = *baud->activity_1e34c;
        uint32_t timebase = *baud->timebase_d9dc;
        uint32_t auxiliary;
        if (activity < 11u)
            auxiliary = (activity * timebase) / 1000u;
        else
            auxiliary = (activity / 10u) *
                        (uint32_t)((int32_t)timebase / 10);
        uart[4] = auxiliary;
    }
    uart[0] |= 0x3401u;
    return true;
}
