#ifndef H9_MAIN_BOARD_TAIL_H
#define H9_MAIN_BOARD_TAIL_H

#include <stdint.h>

#include "h9_main_task_registry.h"

/* Stock app+0x05FFAC..0x05FFE4 follows the memory-window setup with two
 * ROM setup calls, two task requests, and two scheduler handoff calls.
 * Entry addresses must be replacement functions, never stock image pointers. */
typedef struct {
    void *context;
    void (*rom_prepare_391a)(void *context);
    void (*rom_prepare_2cae)(void *context, uintptr_t workspace);
    void (*rom_start_3180)(void *context);
    void (*rom_finish_1650)(void *context);
    H9MainTaskHooks *tasks;
} H9MainBoardTail;

enum { H9_MAIN_BOARD_TASK_WORKSPACE = 0x12850u };

/* Returns -1 for incomplete inputs. Once started, preserves the observed
 * stock call order even if task creation fails; the two task results are
 * returned separately for diagnostics. */
int h9_main_board_tail_run(const H9MainBoardTail *tail,
                           uintptr_t app_core_entry, uintptr_t my_app_entry,
                           int *core_result, int *app_result);
void h9_main_board_tail_bind_target(H9MainBoardTail *tail,
                                    H9MainTaskHooks *tasks);

#endif
