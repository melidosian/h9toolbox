#include "h9_main_uart1_tx_queue.h"

#include <limits.h>
#include <string.h>

int h9_main_uart1_tx_queue_init(H9MainUart1TxQueue *queue,
                                 H9MainUart1Tx *port,
                                 H9MainUart1TxSlot *slots,
                                 size_t slot_count)
{
    size_t index;
    if (queue == NULL || port == NULL || slots == NULL || slot_count == 0u)
        return 0;
    for (index = 0u; index < slot_count; ++index) {
        if (slots[index].storage == NULL || slots[index].dma_address == 0u ||
            slots[index].capacity == 0u || slots[index].capacity > UINT32_MAX)
            return 0;
    }
    memset(queue, 0, sizeof *queue);
    queue->port = port;
    queue->slots = slots;
    queue->slot_count = slot_count;
    for (index = 0u; index < slot_count; ++index) slots[index].length = 0u;
    return 1;
}

void h9_main_uart1_tx_queue_pump(H9MainUart1TxQueue *queue)
{
    H9MainUart1TxSlot *head;
    if (queue == NULL || queue->slots == NULL || queue->slot_count == 0u)
        return;
    if (queue->completed != 0u) {
        queue->completed = 0u;
        if (queue->active && queue->count != 0u) {
            queue->slots[queue->head].length = 0u;
            queue->head = (queue->head + 1u) % queue->slot_count;
            queue->count--;
            queue->active = false;
        }
    }
    if (queue->active || queue->count == 0u) return;
    head = &queue->slots[queue->head];
    if (h9_main_uart1_dma_start(queue->port, head->dma_address,
                                 (uint32_t)head->length)) {
        queue->active = true;
    } else {
        queue->start_failures++;
    }
}

bool h9_main_uart1_tx_queue_enqueue(H9MainUart1TxQueue *queue,
                                     const uint8_t *bytes, size_t length)
{
    H9MainUart1TxSlot *slot;
    size_t tail;
    if (queue == NULL || queue->slots == NULL || queue->slot_count == 0u ||
        (bytes == NULL && length != 0u)) return false;
    h9_main_uart1_tx_queue_pump(queue);
    if (length == 0u) return true;
    if (queue->count >= queue->slot_count) {
        queue->rejected++;
        return false;
    }
    tail = (queue->head + queue->count) % queue->slot_count;
    slot = &queue->slots[tail];
    if (length > slot->capacity || length > UINT32_MAX) {
        queue->rejected++;
        return false;
    }
    memcpy(slot->storage, bytes, length);
    slot->length = length;
    queue->count++;
    h9_main_uart1_tx_queue_pump(queue);
    return true;
}

void h9_main_uart1_tx_queue_event(void *context, unsigned event_number)
{
    H9MainUart1TxQueue *queue = (H9MainUart1TxQueue *)context;
    if (queue != NULL && event_number == 1u) queue->completed = 1u;
}

void h9_main_uart1_tx_queue_write(void *context, const uint8_t *bytes,
                                   size_t length)
{
    (void)h9_main_uart1_tx_queue_enqueue((H9MainUart1TxQueue *)context,
                                          bytes, length);
}
