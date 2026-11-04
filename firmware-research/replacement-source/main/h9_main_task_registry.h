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

#endif
