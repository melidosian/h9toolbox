/* Shared drawing for C modules (gfx.c). */
#ifndef H9_GFX_H
#define H9_GFX_H

#include <stdint.h>

enum { GFX_BLACK = 0x0000, GFX_WHITE = 0xffff, GFX_YELLOW = 0xffe0, GFX_BLUE = 0x001f, GFX_GREY = 0x8410,
       GFX_GREEN = 0x07e0, GFX_RED = 0xf800 };

void gfx_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t color);
/* Draw up to `max` characters of `text` (6 x 8 px cells times `scale`); returns the x after. */
unsigned gfx_text(unsigned x, unsigned y, const char *text, unsigned max, unsigned scale, uint16_t fg, uint16_t bg);
enum { GFX_PROPORTIONAL = 0x100, GFX_MEASURE = 0x200 };

/* Draw text into a pixel budget. Digits retain their six-pixel advance. */
static inline unsigned gfx_label(unsigned x, unsigned y, const char *text, unsigned pixels,
                                 unsigned scale, uint16_t fg, uint16_t bg)
{
#ifdef H9_GFX_TEST_STUB
    return gfx_text(x, y, text, pixels / (6 * scale), scale, fg, bg);
#else
    return gfx_text(x, y, text, pixels, GFX_PROPORTIONAL | scale, fg, bg);
#endif
}

/* Return the number of characters that fit before the width or line break. */
static inline unsigned gfx_label_fit(const char *text, unsigned pixels)
{
#ifdef H9_GFX_TEST_STUB
    unsigned n = 0;
    while (n < pixels / 6 && text[n] && text[n] != '\n') ++n;
    return n;
#else
    return gfx_text(0, 0, text, pixels, GFX_PROPORTIONAL | GFX_MEASURE | 1, 0, 0);
#endif
}
void gfx_wait_release(void);
unsigned gfx_wait_key(void);

#endif
