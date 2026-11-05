#include "h9_main_dev_mg_task.h"

#include <stddef.h>
#include <string.h>

void h9_main_dev_mg_task_bind_target(H9MainDevMgmtTask *task)
{
    if (task == NULL) return;
    memset(task, 0, sizeof *task);
    task->ready_11798 = (volatile uint8_t *)(uintptr_t)0x00011798u;
}

bool h9_main_dev_mg_task_start(H9MainDevMgmtTask *task,
                                const H9MainDevMgmtHooks *hooks)
{
    static const char *const volumes[] = {
        "sdfile", "fat_nor", "res_nor"
    };
    unsigned index;
    if (task == NULL || task->ready_11798 == NULL || task->started ||
        hooks == NULL || hooks->initialize_devices == NULL ||
        hooks->register_volume == NULL ||
        hooks->acknowledge_queue == NULL ||
        hooks->receive_message == NULL) return false;

    memset(task->message, 0, sizeof task->message);
    *task->ready_11798 = 1u;
    hooks->initialize_devices(hooks->context);
    for (index = 0u; index < sizeof volumes / sizeof volumes[0]; ++index)
        hooks->register_volume(hooks->context, volumes[index]);
    hooks->acknowledge_queue(hooks->context, 0x00012F90u);
    task->started = true;
    return true;
}

bool h9_main_dev_mg_task_step(H9MainDevMgmtTask *task,
                               const H9MainDevMgmtHooks *hooks)
{
    if (task == NULL || !task->started || hooks == NULL ||
        hooks->receive_message == NULL) return false;
    hooks->receive_message(hooks->context, task->message);
    return true;
}
