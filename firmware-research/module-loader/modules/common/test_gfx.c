#include <assert.h>
#include <stdio.h>
#include "gfx.h"
#include "h9mod.h"

static unsigned width, height, bytes, windows;

void h9_lcd_window(unsigned x0, unsigned y0, unsigned x1, unsigned y1)
{
    assert(x0 <= x1 && x1 < H9_WIDTH && y0 <= y1 && y1 < H9_HEIGHT);
    assert(bytes == width * height * 2);
    width = x1 - x0 + 1;
    height = y1 - y0 + 1;
    bytes = 0;
    ++windows;
}

void h9_lcd_write(const void *buffer, unsigned length)
{
    assert(buffer && length <= 128 && !(length & 1));
    bytes += length;
    assert(bytes <= width * height * 2);
}

unsigned h9_key(void) { return 0; }
void h9_delay(unsigned ms) { (void)ms; }

int main(void)
{
    assert(gfx_label_fit("ILLINOIS LINK REPEATER", 117) == 21);
    assert(gfx_label_fit("ILLINOIS LINK REPEATER", 118) == 22);
    assert(gfx_label_fit("!I 0\nOTHER", 100) == 4 && !windows);
    assert(gfx_label(3, 0, "!I 0", 15, 1, GFX_WHITE, GFX_BLACK) == 18);
    assert(windows == 4 && width == 6 && height == 8);
    assert(gfx_label(154, 0, "12", 40, 1, GFX_WHITE, GFX_BLACK) == 160);
    assert(windows == 5);
    assert(gfx_text(3, 16, "I. 1", 4, 2, GFX_WHITE, GFX_BLACK) == 51);
    assert(windows == 9 && width == 12 && height == 16 && bytes == width * height * 2);
    puts("Proportional fit, LCD clipping, tabular digits, and fixed-cell compatibility passed.");
}
