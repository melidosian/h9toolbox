#ifndef H9_MAIN_HEAP_BITMAP_H
#define H9_MAIN_HEAP_BITMAP_H

#include <stdbool.h>
#include <stdint.h>

#define H9_MAIN_HEAP_BITMAP_WORDS 55u

/* First one-time allocation bitmap step at stock app+0x05FA8A..0x05FB32.
 * The two ranges are expressed as byte addresses and marked in 128-byte
 * units. This does not implement the later allocator metadata or clock setup. */
typedef struct {
    volatile uint8_t *initialized;
    volatile uint32_t *words;
} H9MainHeapBitmap;

void h9_main_heap_bitmap_bind_target(H9MainHeapBitmap *bitmap);
bool h9_main_heap_bitmap_initialize(H9MainHeapBitmap *bitmap);

#endif
