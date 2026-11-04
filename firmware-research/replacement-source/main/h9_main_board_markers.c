#include "h9_main_board_markers.h"

#include <stddef.h>

void h9_main_board_markers_bind_target(H9MainBoardMarkers *markers)
{
    if (markers == NULL) return;
    markers->ram_code_guard = (volatile uint8_t *)(uintptr_t)0x0000DA80u;
    markers->stack_guard = (volatile uint8_t *)(uintptr_t)0x0000E290u;
    markers->state_da58 = (volatile uint8_t *)(uintptr_t)0x0000DA58u;
    markers->state_da5c = (volatile uint8_t *)(uintptr_t)0x0000DA5Cu;
    markers->state_d9d0 = (volatile uint8_t *)(uintptr_t)0x0000D9D0u;
}

bool h9_main_board_markers_initialize(H9MainBoardMarkers *markers)
{
    unsigned index;
    if (markers == NULL || markers->ram_code_guard == NULL ||
        markers->stack_guard == NULL || markers->state_da58 == NULL ||
        markers->state_da5c == NULL || markers->state_d9d0 == NULL)
        return false;
    for (index = 0u; index < H9_MAIN_GUARD_BYTES; ++index) {
        markers->ram_code_guard[index] = 0x5Au;
        markers->stack_guard[index] = 0x5Au;
    }
    /* Stock helpers app+0x0C5472 and app+0x0C5486 follow the guard stores. */
    *markers->state_da58 = 1u;
    *markers->state_da5c = 1u;
    *markers->state_d9d0 = 1u;
    return true;
}
