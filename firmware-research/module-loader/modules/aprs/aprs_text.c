/* aprs_wrap (aprs_core.h), apart from the core so screens that do not wrap
 * text (list, map) do not carry it. */
#include "aprs_core.h"

unsigned aprs_wrap(unsigned y, const char *text, unsigned limit, uint16_t fg)
{
    char b[27];
    for (unsigned at = 0; text[at] && y + 8 <= limit; at += 26, y += 10) {
        unsigned n = 0;
        for (; n < 26 && text[at + n]; n++) b[n] = text[at + n];
        b[n] = 0;
        aprs_line(y, b, fg, GFX_BLACK);
        if (n < 26) return y + 10;
    }
    return y;
}
