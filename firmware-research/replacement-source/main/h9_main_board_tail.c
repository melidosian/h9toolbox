#include "h9_main_board_tail.h"

#include <stddef.h>

int h9_main_board_tail_run(const H9MainBoardTail *tail,
                           uintptr_t app_core_entry, uintptr_t my_app_entry,
                           int *core_result, int *app_result)
{
    if (tail == NULL || tail->rom_prepare_391a == NULL ||
        tail->rom_prepare_2cae == NULL || tail->rom_start_3180 == NULL ||
        tail->rom_finish_1650 == NULL || tail->tasks == NULL ||
        tail->tasks->create == NULL || app_core_entry == 0u ||
        my_app_entry == 0u || core_result == NULL || app_result == NULL)
        return -1;

    tail->rom_prepare_391a(tail->context);
    tail->rom_prepare_2cae(tail->context, H9_MAIN_BOARD_TASK_WORKSPACE);
    *core_result = h9_main_task_launch(tail->tasks, app_core_entry, 0u,
                                       "app_core");
    *app_result = h9_main_task_launch(tail->tasks, my_app_entry, 0u,
                                      "my_app");
    tail->rom_start_3180(tail->context);
    tail->rom_finish_1650(tail->context);
    return 0;
}

static void target_prepare_391a(void *context)
{
    typedef void (*RomPrepare)(void);
    (void)context;
    ((RomPrepare)(uintptr_t)0x0200391Au)();
}

static void target_prepare_2cae(void *context, uintptr_t workspace)
{
    typedef void (*RomPrepare)(uintptr_t);
    (void)context;
    ((RomPrepare)(uintptr_t)0x02002CAEu)(workspace);
}

static void target_start_3180(void *context)
{
    typedef void (*RomStart)(void);
    (void)context;
    ((RomStart)(uintptr_t)0x02003180u)();
}

static void target_finish_1650(void *context)
{
    typedef void (*RomFinish)(void);
    (void)context;
    ((RomFinish)(uintptr_t)0x02001650u)();
}

void h9_main_board_tail_bind_target(H9MainBoardTail *tail,
                                    H9MainTaskHooks *tasks)
{
    if (tail == NULL) return;
    tail->context = NULL;
    tail->rom_prepare_391a = target_prepare_391a;
    tail->rom_prepare_2cae = target_prepare_2cae;
    tail->rom_start_3180 = target_start_3180;
    tail->rom_finish_1650 = target_finish_1650;
    tail->tasks = tasks;
}
