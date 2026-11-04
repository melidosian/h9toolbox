#include "h9_main_sys_event.h"

#include <stddef.h>

static void store32(volatile uint8_t *bytes, uint32_t offset, uint32_t value)
{
    unsigned index;
    for (index = 0u; index < 4u; ++index)
        bytes[offset + index] = (uint8_t)(value >> (index * 8u));
}

static uint32_t load32(const volatile uint8_t *bytes, uint32_t offset)
{
    unsigned index;
    uint32_t value = 0u;
    for (index = 0u; index < 4u; ++index)
        value |= (uint32_t)bytes[offset + index] << (index * 8u);
    return value;
}

void h9_main_sys_event_bind_state(H9MainSysEvent *event)
{
    if (event == NULL) return;
    event->state_0xc070 = (volatile uint8_t *)(uintptr_t)0x0000C070u;
}

int h9_main_sys_event_init(const H9MainSysEvent *event)
{
    volatile uint8_t *state;
    H9MainSysEventBlock nodes = { NULL, 0u };
    H9MainSysEventBlock buffer = { NULL, 0u };
    uint32_t head = 0x0000C08Cu;
    uint32_t previous;
    unsigned index;
    if (event == NULL || event->state_0xc070 == NULL ||
        event->allocate == NULL || event->init_buffer == NULL ||
        event->task_hooks == NULL ||
        event->event_task_entry == 0u) return H9_MAIN_TASK_LAUNCH_ERROR;
    state = event->state_0xc070;
    if (state[0] != 0u) return 0;
    state[0] = 1u;
    state[5] = 0xFFu;
    store32(state, 0x0Cu, 0u);
    store32(state, 0x18u, 0u);
    store32(state, 0x1Cu, head);
    store32(state, 0x20u, head);
    if (!event->allocate(event->context, 0x38u, &nodes) ||
        nodes.bytes == NULL || nodes.address == 0u ||
        nodes.address > UINT32_MAX - 0x38u)
        return H9_MAIN_SYS_EVENT_ALLOC_ERROR;
    for (index = 0u; index < 2u; ++index) {
        uint32_t offset = index * 0x1Cu;
        uint32_t address = nodes.address + offset;
        previous = load32(state, 0x20u);
        nodes.bytes[offset + 0x14u] = 1u;
        store32(nodes.bytes, offset + 0x0Cu, 0u);
        store32(state, 0x20u, address);
        store32(nodes.bytes, offset, head);
        store32(nodes.bytes, offset + 4u, previous);
        if (index == 0u) store32(state, 0x1Cu, address);
        else store32(nodes.bytes, 0u, address);
    }
    if (load32(state, 0x10u) == 0u) {
        if (event->allocate(event->context, 0x124u, &buffer) &&
            buffer.bytes != NULL && buffer.address != 0u &&
            buffer.address <= UINT32_MAX - 0x124u) {
            store32(state, 0x10u, buffer.address);
            event->init_buffer(event->context,
                               buffer.address + 0x24u, 0x100u);
        }
    }
    (void)h9_main_task_launch(event->task_hooks, event->event_task_entry,
                              0u, "sys_event");
    return 0;
}

int h9_main_sys_event_task_prelude(const H9MainSysEventTaskPrelude *task)
{
    H9MainSysEventBlock buffer = { NULL, 0u };
    uint8_t workspace[28] = { 0u };
    if (task == NULL || task->state_0xc070 == NULL ||
        task->allocate == NULL || task->prepare_region == NULL ||
        task->enter_event_loop == NULL) return H9_MAIN_TASK_LAUNCH_ERROR;
    if (!task->allocate(task->context, 0x50u, &buffer) ||
        buffer.bytes == NULL || buffer.address == 0u) {
        if (task->allocation_failure != NULL)
            task->allocation_failure(task->context);
        /* The stock failure helper may not return. Until its behavior is
         * known, the replacement cannot hand a null buffer to the ROM. */
        return H9_MAIN_SYS_EVENT_ALLOC_ERROR;
    }
    task->prepare_region(task->context, buffer.address);
    store32(task->state_0xc070, 0x14u, buffer.address);
    task->enter_event_loop(task->context, workspace, buffer.address,
                           0x00011770u, 0x01E2DC44u, 0x01E2DC5Cu, 0u);
    return 0;
}
