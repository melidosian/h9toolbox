#include "h9_main_aprs_uart1_link.h"

#include <string.h>

static H9MainAprsUart1Link *as_link(void *context)
{
    return (H9MainAprsUart1Link *)context;
}

static void prepare_tx(void *context, uint32_t descriptor)
{
    H9MainAprsUart1Link *link = as_link(context);
    link->dma.prepare_tx(link->dma.context, descriptor);
}

static void enable_tx(void *context, uint32_t descriptor, uint32_t option)
{
    H9MainAprsUart1Link *link = as_link(context);
    link->dma.enable_tx(link->dma.context, descriptor, option);
}

static void acknowledge_tx(void *context)
{
    H9MainAprsUart1Link *link = as_link(context);
    link->dma.acknowledge_tx(link->dma.context);
}

static void acknowledge_rx(void *context)
{
    H9MainAprsUart1Link *link = as_link(context);
    link->dma.acknowledge_rx(link->dma.context);
}

static void irq_event(void *context, unsigned event_number)
{
    H9MainAprsUart1Link *link = as_link(context);
    h9_main_uart1_tx_queue_event(&link->transmit_queue, event_number);
}

static size_t serial_read(void *context, uint8_t *bytes, size_t capacity)
{
    H9MainAprsUart1Link *link = as_link(context);
    return h9_main_serial_dma_read(&link->receive, bytes, capacity);
}

static void serial_write(void *context, const uint8_t *bytes, size_t length)
{
    H9MainAprsUart1Link *link = as_link(context);
    h9_main_uart1_tx_queue_write(&link->transmit_queue, bytes, length);
}

static void packet(void *context, const H9MainAx25Decoded *value)
{
    H9MainAprsUart1Link *link = as_link(context);
    if (link->application.packet != NULL)
        link->application.packet(link->application_context, value);
}

static void gps_sentence(void *context, const uint8_t *bytes, size_t length)
{
    H9MainAprsUart1Link *link = as_link(context);
    if (link->application.gps_sentence != NULL)
        link->application.gps_sentence(link->application_context,
                                       bytes, length);
}

static void control(void *context, H9MainControlAction value)
{
    H9MainAprsUart1Link *link = as_link(context);
    if (link->application.control != NULL)
        link->application.control(link->application_context, value);
}

static void version(void *context, const H9MainAprsVersions *value)
{
    H9MainAprsUart1Link *link = as_link(context);
    if (link->application.version != NULL)
        link->application.version(link->application_context, value);
}

int h9_main_aprs_uart1_link_init(
    H9MainAprsUart1Link *link, volatile uint32_t *uart_registers,
    const uint8_t *receive_storage, uint32_t receive_capacity,
    uint32_t receive_chunk_bytes, H9MainUart1TxSlot *transmit_slots,
    size_t transmit_slot_count, const H9MainAprsUart1DmaHooks *dma,
    const H9MainAprsSessionCallbacks *application,
    void *application_context)
{
    H9MainAprsSessionCallbacks callbacks;
    if (link == NULL || uart_registers == NULL || receive_storage == NULL ||
        receive_capacity == 0u ||
        (receive_capacity & (receive_capacity - 1u)) != 0u ||
        receive_chunk_bytes == 0u ||
        receive_chunk_bytes > receive_capacity || dma == NULL ||
        dma->prepare_tx == NULL || dma->enable_tx == NULL ||
        dma->acknowledge_tx == NULL || dma->acknowledge_rx == NULL)
        return 0;
    memset(link, 0, sizeof *link);
    memset(&callbacks, 0, sizeof callbacks);
    link->dma = *dma;
    if (application != NULL) link->application = *application;
    link->application_context = application_context;
    link->receive.storage = receive_storage;
    link->receive.capacity = receive_capacity;
    link->transmit.registers = uart_registers;
    link->transmit.prepare_dma = prepare_tx;
    link->transmit.enable_dma = enable_tx;
    link->transmit.context = link;
    if (!h9_main_uart1_tx_queue_init(&link->transmit_queue,
                                      &link->transmit, transmit_slots,
                                      transmit_slot_count)) return 0;
    link->irq.registers = uart_registers;
    link->irq.receive = &link->receive;
    link->irq.receive_chunk_bytes = receive_chunk_bytes;
    link->irq.acknowledge_tx_dma = acknowledge_tx;
    link->irq.acknowledge_rx_dma = acknowledge_rx;
    link->irq.event = irq_event;
    link->irq.context = link;
    callbacks.packet = packet;
    callbacks.gps_sentence = gps_sentence;
    callbacks.control = control;
    callbacks.version = version;
    callbacks.serial_read = serial_read;
    callbacks.serial_write = serial_write;
    h9_main_aprs_session_init(&link->session, &callbacks, link);
    return 1;
}

size_t h9_main_aprs_uart1_link_poll(H9MainAprsUart1Link *link)
{
    size_t processed;
    if (link == NULL) return 0u;
    h9_main_uart1_tx_queue_pump(&link->transmit_queue);
    processed = h9_main_aprs_session_poll(&link->session);
    h9_main_uart1_tx_queue_pump(&link->transmit_queue);
    return processed;
}

void h9_main_aprs_uart1_link_irq(H9MainAprsUart1Link *link)
{
    if (link != NULL) h9_main_uart1_irq(&link->irq);
}
