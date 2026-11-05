#ifndef H9_MAIN_UART1_DMA_ROM_H
#define H9_MAIN_UART1_DMA_ROM_H

#include <stdbool.h>
#include <stdint.h>

#include "h9_main_aprs_uart1_link.h"

/* Stock v1.0.38 ROM calls used by the UART1 bulk writer and IRQ. The ROM
 * entry points are mapped, but target execution has not been validated. */
typedef struct {
    void (*initialize)(void *context, uint32_t descriptor_address);
    void (*prepare)(void *context, uint32_t descriptor_address);
    void (*enable)(void *context, uint32_t descriptor_address,
                   uint32_t option);
    void (*acknowledge)(void *context, uint32_t descriptor_address);
    void *context;
} H9MainUart1DmaRom;

void h9_main_uart1_dma_rom_bind_target(H9MainUart1DmaRom *rom);

/* UART1 open calls this once for RX then once for TX. The separate board
 * adapter must still arrange clocks, IRQ registration, and DMA storage. */
bool h9_main_uart1_dma_rom_initialize(H9MainUart1DmaRom *rom,
                                       uint32_t descriptor_address);

/* rom must outlive hooks and the UART1 link that receives them. */
bool h9_main_uart1_dma_rom_make_hooks(H9MainAprsUart1DmaHooks *hooks,
                                       H9MainUart1DmaRom *rom);

#endif
