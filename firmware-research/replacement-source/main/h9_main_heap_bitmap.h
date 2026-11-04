#ifndef H9_MAIN_HEAP_BITMAP_H
#define H9_MAIN_HEAP_BITMAP_H

#include <stdbool.h>
#include <stdint.h>

#define H9_MAIN_HEAP_BITMAP_WORDS 55u

/* First one-time allocation bitmap and metadata stage at stock
 * app+0x05FA8A..0x05FB64.
 * The two ranges are expressed as byte addresses and marked in 128-byte
 * units. The next memory-window stage is a separate module. */
typedef struct {
    volatile uint8_t *initialized;
    volatile uint32_t *words;
} H9MainHeapBitmap;

void h9_main_heap_bitmap_bind_target(H9MainHeapBitmap *bitmap);
bool h9_main_heap_bitmap_initialize(H9MainHeapBitmap *bitmap);

#endif
