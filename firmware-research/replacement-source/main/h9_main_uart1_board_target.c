#include "h9_main_uart1_board_target.h"

#include <stddef.h>

static H9MainUart1BoardTarget *active_uart1;

void h9_main_uart1_board_target_vector(void)
{
    H9MainUart1BoardTarget *target = active_uart1;
    if (target != NULL && target->handler != NULL)
        target->handler(target->handler_context);
}

void h9_main_uart1_board_target_bind(H9MainUart1BoardTarget *target)
{
    if (target == NULL) return;
    h9_main_uart1_pins_bind_target(&target->pins);
    h9_main_uart1_dma_rom_bind_target(&target->dma);
    h9_main_irq_registration_bind_target(&target->irq);
    h9_main_uart1_baud_bind_target(&target->baud);
    target->handler = NULL;
    target->handler_context = NULL;
}

static bool configure_pins(void *context, uint8_t tx_pin, uint8_t rx_pin)
{
    H9MainUart1BoardTarget *target = (H9MainUart1BoardTarget *)context;
    return h9_main_uart1_pins_configure(&target->pins, tx_pin, rx_pin);
}

static bool initialize_dma(void *context, uint32_t descriptor)
{
    H9MainUart1BoardTarget *target = (H9MainUart1BoardTarget *)context;
    return h9_main_uart1_dma_rom_initialize(&target->dma, descriptor);
}

static bool register_irq(void *context, uint8_t number, uint8_t priority,
                         void (*handler)(void *), void *handler_context)
{
    H9MainUart1BoardTarget *target = (H9MainUart1BoardTarget *)context;
    H9MainUart1BoardTarget *previous = active_uart1;
    if (number != 0x13u || priority != 3u || handler == NULL ||
        handler_context == NULL ||
        (previous != NULL && previous != target)) return false;
    target->handler = handler;
    target->handler_context = handler_context;
    active_uart1 = target;
    if (!h9_main_irq_register(&target->irq, number, priority,
                               h9_main_uart1_board_target_vector)) {
        target->handler = NULL;
        target->handler_context = NULL;
        active_uart1 = previous;
        return false;
    }
    return true;
}

static bool configure_baud(void *context, uint32_t half_baud)
{
    H9MainUart1BoardTarget *target = (H9MainUart1BoardTarget *)context;
    return h9_main_uart1_baud_configure(&target->baud, half_baud);
}

bool h9_main_uart1_board_target_open(H9MainUart1BoardTarget *target,
                                      H9MainAprsUart1Link *link,
                                      const H9MainSerialOpenRecord *record)
{
    H9MainUart1BoardHooks hooks;
    H9MainAprsUart1DmaHooks expected_dma;
#if UINTPTR_MAX == UINT32_MAX
    size_t slot;
#endif
    if (target == NULL || link == NULL || record == NULL ||
        target->pins.pinmux_1e5100 == NULL ||
        target->pins.gpio_1e5080 == NULL ||
        target->dma.initialize == NULL ||
        target->dma.prepare == NULL || target->dma.enable == NULL ||
        target->dma.acknowledge == NULL ||
        target->irq.vector_words_2bf00 == NULL ||
        target->irq.control_words_10f100 == NULL ||
        target->irq.enter_02001634 == NULL ||
        target->irq.leave_02001650 == NULL ||
        target->baud.uart_registers == NULL ||
        target->baud.clock_ready_d9e4 == NULL ||
        target->baud.timebase_d9dc == NULL ||
        target->baud.activity_1e34c == NULL ||
        *target->baud.clock_ready_d9e4 == 0u ||
        link->irq.registers != target->baud.uart_registers ||
        link->transmit_queue.slots == NULL ||
        link->transmit_queue.slot_count == 0u ||
        link->dma.context != &target->dma ||
        (active_uart1 != NULL && active_uart1 != target)) return false;
    if (!h9_main_uart1_dma_rom_make_hooks(&expected_dma, &target->dma) ||
        link->dma.prepare_tx != expected_dma.prepare_tx ||
        link->dma.enable_tx != expected_dma.enable_tx ||
        link->dma.acknowledge_tx != expected_dma.acknowledge_tx ||
        link->dma.acknowledge_rx != expected_dma.acknowledge_rx)
        return false;
#if UINTPTR_MAX == UINT32_MAX
    if ((uintptr_t)link->receive.storage != record->storage_address)
        return false;
    for (slot = 0u; slot < link->transmit_queue.slot_count; ++slot) {
        H9MainUart1TxSlot *tx_slot =
            &link->transmit_queue.slots[slot];
        if ((uintptr_t)tx_slot->storage != tx_slot->dma_address)
            return false;
    }
#endif

    hooks.configure_pins = configure_pins;
    hooks.initialize_dma = initialize_dma;
    hooks.register_irq = register_irq;
    hooks.configure_baud = configure_baud;
    hooks.context = target;
    return h9_main_uart1_board_open(link, record, &hooks) != 0;
}
