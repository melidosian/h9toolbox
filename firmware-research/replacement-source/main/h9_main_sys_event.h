#ifndef H9_MAIN_SYS_EVENT_H
#define H9_MAIN_SYS_EVENT_H

#include <stdbool.h>
#include <stdint.h>

#include "h9_main_task_registry.h"

/* Stock app+0x02D4D8 uses address-valued links in 32-bit RAM. An allocator
 * supplies both a writable view and the address stored in those links. */
typedef struct {
    volatile uint8_t *bytes;
    uint32_t address;
} H9MainSysEventBlock;

typedef struct {
    volatile uint8_t *state_0xc070; /* at least 0x24 bytes */
    void *context;
    bool (*allocate)(void *context, uint32_t size,
                     H9MainSysEventBlock *block);
    void (*init_buffer)(void *context, uint32_t address, uint32_t length);
    const H9MainTaskHooks *task_hooks;
    uintptr_t event_task_entry;
} H9MainSysEvent;

enum { H9_MAIN_SYS_EVENT_ALLOC_ERROR = -12 };

void h9_main_sys_event_bind_state(H9MainSysEvent *event);
int h9_main_sys_event_init(const H9MainSysEvent *event);

/* Stock task entry 0x01E2D642 (app+0x02D542) prepares a 0x50-byte
 * allocation, stores it at RAM 0xC084, then hands control to ROM 0x02001634.
 * The ROM event-loop contract is not recovered, so this is a prelude only. */
typedef struct {
    volatile uint8_t *state_0xc070; /* at least 0x18 bytes */
    void *context;
    bool (*allocate)(void *context, uint32_t size,
                     H9MainSysEventBlock *block);
    void (*allocation_failure)(void *context);
    void (*prepare_region)(void *context, uint32_t address);
    void (*enter_event_loop)(void *context, uint8_t workspace[28],
                             uint32_t buffer_address, uint32_t event_state,
                             uint32_t table_a, uint32_t table_b,
                             uint32_t zero_register);
} H9MainSysEventTaskPrelude;

int h9_main_sys_event_task_prelude(const H9MainSysEventTaskPrelude *task);

#endif
