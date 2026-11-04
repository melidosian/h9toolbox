#include "h9_main_heap_bitmap.h"

#include <stddef.h>

#define H9_MAIN_HEAP_LEVEL1_WORD 48u
#define H9_MAIN_HEAP_BLOCK_SHIFT 7u

static void mark_blocks(volatile uint32_t *words,
                        uint32_t first, uint32_t last)
{
    uint32_t block;
    for (block = first; block < last; ++block) {
        words[block >> 5] |= 1u << (block & 31u);
        words[H9_MAIN_HEAP_LEVEL1_WORD + (block >> 10)] |=
            1u << ((block >> 5) & 31u);
    }
}

void h9_main_heap_bitmap_bind_target(H9MainHeapBitmap *bitmap)
{
    if (bitmap == NULL) return;
    bitmap->initialized = (volatile uint8_t *)(uintptr_t)0x0000D5F0u;
    bitmap->words = (volatile uint32_t *)(uintptr_t)0x0001D7D0u;
}

bool h9_main_heap_bitmap_initialize(H9MainHeapBitmap *bitmap)
{
    unsigned index;
    if (bitmap == NULL || bitmap->initialized == NULL ||
        bitmap->words == NULL) return false;
    if (*bitmap->initialized != 0u) return true;
    *bitmap->initialized = 1u;
    /* Stock ROM 0x021127B4 clears 0xDC bytes at 0x1D7D0 before bitset. */
    for (index = 0u; index < H9_MAIN_HEAP_BITMAP_WORDS; ++index)
        bitmap->words[index] = 0u;
    /* Stock rounds up the first start, but floors the second start. */
    mark_blocks(bitmap->words,
                (0x0001E4D8u + 127u) >> H9_MAIN_HEAP_BLOCK_SHIFT,
                0x0002BF00u >> H9_MAIN_HEAP_BLOCK_SHIFT);
    mark_blocks(bitmap->words,
                0x0002D27Fu >> H9_MAIN_HEAP_BLOCK_SHIFT,
                0x0002FF80u >> H9_MAIN_HEAP_BLOCK_SHIFT);
    /* Stock app+0x05FB32..0x05FB62 writes these fields at 0x1D898.
     * The parallel shift/store at app+0x05FB4E stores the old r0 value
     * (0x10780) before r0 becomes the first block number (0x3CA). */
    bitmap->words[50] = 0x0001E500u;
    bitmap->words[51] = 0x0002FF80u;
    bitmap->words[52] = 0x00010780u;
    bitmap->words[53] = (0x05FFu << 16) | 0x03CAu;
    bitmap->words[54] = 0x020Fu << 16;
    return true;
}
