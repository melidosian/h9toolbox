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

#endif
