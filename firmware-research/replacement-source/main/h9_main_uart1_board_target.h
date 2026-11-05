#ifndef H9_MAIN_UART1_BOARD_TARGET_H
#define H9_MAIN_UART1_BOARD_TARGET_H

#include <stdbool.h>

#include "h9_main_irq_registration.h"
#include "h9_main_uart1_baud.h"
#include "h9_main_uart1_board.h"
#include "h9_main_uart1_dma_rom.h"
#include "h9_main_uart1_pins.h"

/* One UART1 vector exists on this target. Keep this object and link alive
 * after opening; the interrupt bridge retains their addresses. */
typedef struct {
    H9MainUart1Pins pins;
    H9MainUart1DmaRom dma;
    H9MainIrqRegistration irq;
    H9MainUart1Baud baud;
    void (*handler)(void *);
    void *handler_context;
} H9MainUart1BoardTarget;

void h9_main_uart1_board_target_bind(H9MainUart1BoardTarget *target);

/* Before any MMIO, requires the link's DMA hooks to refer to target->dma,
 * the UART register pointers to match, and a nonzero clock state. On Pi32,
 * receive and transmit DMA addresses must equal their backing pointers. */
bool h9_main_uart1_board_target_open(H9MainUart1BoardTarget *target,
                                      H9MainAprsUart1Link *link,
                                      const H9MainSerialOpenRecord *record);

/* No-argument function stored in the Pi32 IRQ vector table. */
void h9_main_uart1_board_target_vector(void);

#endif
