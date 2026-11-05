#ifndef H9_MAIN_DEV_MG_TASK_H
#define H9_MAIN_DEV_MG_TASK_H

#include <stdbool.h>
#include <stdint.h>

/* Stock app+0x04A870 creates this task as "dev_mg". The 32-byte message
 * buffer is initialized once and reused by the blocking ROM receive call. */
typedef struct {
    volatile uint8_t *ready_11798;
    uint32_t message[8];
    bool started;
} H9MainDevMgmtTask;

typedef struct {
    /* Stock app+0x02C2BC walks the built-in device initializer table. */
    void (*initialize_devices)(void *context);
    /* Stock app+0x04A7B2 resolves a name and inserts an opened device. */
    void (*register_volume)(void *context, const char *name);
    /* Stock calls ROM 0x02002722 with descriptor 0x12F90. */
    void (*acknowledge_queue)(void *context, uint32_t descriptor);
    /* Stock calls ROM 0x02002CD8 repeatedly with the 32-byte buffer. */
    void (*receive_message)(void *context, uint32_t message[8]);
    void *context;
} H9MainDevMgmtHooks;

void h9_main_dev_mg_task_bind_target(H9MainDevMgmtTask *task);
/* Begins the stock task sequence. Returns false without effects when a
 * required binding is absent; device registration results are ignored by
 * the stock task. This does not start the scheduler or call the ROM. */
bool h9_main_dev_mg_task_start(H9MainDevMgmtTask *task,
                                const H9MainDevMgmtHooks *hooks);
/* Performs one iteration of stock's endless blocking receive loop. */
bool h9_main_dev_mg_task_step(H9MainDevMgmtTask *task,
                               const H9MainDevMgmtHooks *hooks);

#endif
