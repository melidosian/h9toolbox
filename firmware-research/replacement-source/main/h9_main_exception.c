#include "h9_main_exception.h"

#include <stddef.h>

volatile uint32_t h9_main_last_exception[25];

void h9_main_exception_capture(const uint32_t *frame)
{
    unsigned index;
    if (frame == NULL) return;
    for (index = 0; index < H9_MAIN_EXCEPTION_FRAME_WORDS; ++index)
        h9_main_last_exception[index + 1u] = frame[index];
    h9_main_last_exception[0] = 0x48394654u; /* "H9FT" */
}
