#include "h9_main_uart1_dma_rom.h"

#include <stddef.h>

#define UART1_TX_DESCRIPTOR 0x001E3B0u
#define UART1_RX_DESCRIPTOR 0x001E360u
#define ROM_DMA_PREPARE 0x02002C80u
#define ROM_DMA_ENABLE 0x02002B66u
#define ROM_DMA_ACKNOWLEDGE 0x02002722u

static void target_prepare(void *context, uint32_t descriptor)
{
    void (*operation)(uint32_t) =
        (void (*)(uint32_t))(uintptr_t)ROM_DMA_PREPARE;
    (void)context;
    operation(descriptor);
}

static void target_enable(void *context, uint32_t descriptor, uint32_t option)
{
    void (*operation)(uint32_t, uint32_t) =
        (void (*)(uint32_t, uint32_t))(uintptr_t)ROM_DMA_ENABLE;
    (void)context;
    operation(descriptor, option);
}

static void target_acknowledge(void *context, uint32_t descriptor)
{
    void (*operation)(uint32_t) =
        (void (*)(uint32_t))(uintptr_t)ROM_DMA_ACKNOWLEDGE;
    (void)context;
    operation(descriptor);
}

void h9_main_uart1_dma_rom_bind_target(H9MainUart1DmaRom *rom)
{
    if (rom == NULL) return;
    rom->prepare = target_prepare;
    rom->enable = target_enable;
    rom->acknowledge = target_acknowledge;
    rom->context = NULL;
}

static void prepare_tx(void *context, uint32_t descriptor)
{
    H9MainUart1DmaRom *rom = (H9MainUart1DmaRom *)context;
    rom->prepare(rom->context, descriptor);
}

static void enable_tx(void *context, uint32_t descriptor, uint32_t option)
{
    H9MainUart1DmaRom *rom = (H9MainUart1DmaRom *)context;
    rom->enable(rom->context, descriptor, option);
}

static void acknowledge_tx(void *context)
{
    H9MainUart1DmaRom *rom = (H9MainUart1DmaRom *)context;
    rom->acknowledge(rom->context, UART1_TX_DESCRIPTOR);
}

static void acknowledge_rx(void *context)
{
    H9MainUart1DmaRom *rom = (H9MainUart1DmaRom *)context;
    rom->acknowledge(rom->context, UART1_RX_DESCRIPTOR);
}

bool h9_main_uart1_dma_rom_make_hooks(H9MainAprsUart1DmaHooks *hooks,
                                       H9MainUart1DmaRom *rom)
{
    if (hooks == NULL || rom == NULL || rom->prepare == NULL ||
        rom->enable == NULL || rom->acknowledge == NULL)
        return false;
    hooks->prepare_tx = prepare_tx;
    hooks->enable_tx = enable_tx;
    hooks->acknowledge_tx = acknowledge_tx;
    hooks->acknowledge_rx = acknowledge_rx;
    hooks->context = rom;
    return true;
}
