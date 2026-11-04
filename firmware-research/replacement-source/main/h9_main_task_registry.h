#ifndef H9_MAIN_TASK_REGISTRY_H
#define H9_MAIN_TASK_REGISTRY_H

#include <stddef.h>
#include <stdint.h>

/* The 12-byte stock records at app+0x0A2A64 name tasks and carry three
 * creation parameters. Their physical meanings are not established yet. */
typedef struct {
    const char *name;
    uint8_t byte_4;
    uint16_t halfword_6;
    uint16_t halfword_8;
} H9MainTaskDescriptor;

enum { H9_MAIN_TASK_COUNT = 27 };

extern const H9MainTaskDescriptor h9_main_tasks[H9_MAIN_TASK_COUNT];
const H9MainTaskDescriptor *h9_main_task_find(const char *name);

typedef struct {
    void *context;
    int (*create)(void *context, uintptr_t entry, uintptr_t argument,
                  uint8_t byte_4, uint16_t halfword_6,
                  uint16_t halfword_8, const char *name);
    void (*report_failure)(void *context);
} H9MainTaskHooks;

enum { H9_MAIN_TASK_LAUNCH_ERROR = -14 };

/* Stock app+0x02CF22 looks up the name, calls ROM 0x02003102 with the
 * record's three fields, and converts a missing or failed creation to -14.
 * The entrypoint and its argument are supplied by the caller. */
int h9_main_task_launch(const H9MainTaskHooks *hooks,
                        uintptr_t entry, uintptr_t argument,
                        const char *name);
void h9_main_task_hooks_bind_target(H9MainTaskHooks *hooks);

#endif
