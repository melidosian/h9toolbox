#ifndef H9_MAIN_UART1_BAUD_H
#define H9_MAIN_UART1_BAUD_H

#include <stdbool.h>
#include <stdint.h>

/* Stock main app+0x0C56E4. The pointers are injectable for host tests; the
 * target binding identifies observed addresses but does not initialize clocks. */
typedef struct {
    volatile uint32_t *uart_registers;
    volatile uint32_t *clock_ready_d9e4;
    volatile uint32_t *timebase_d9dc;
    volatile uint32_t *activity_1e34c;
} H9MainUart1Baud;

void h9_main_uart1_baud_bind_target(H9MainUart1Baud *baud);

/* half_baud is (serial record baud & 0x1FFFFFE) >> 1. Returns false before
 * touching UART MMIO if prerequisites are absent. */
bool h9_main_uart1_baud_configure(H9MainUart1Baud *baud,
                                  uint32_t half_baud);

#endif
