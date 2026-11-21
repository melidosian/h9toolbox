/* aprs_wrap (aprs_core.h), apart from the core so screens that do not wrap
 * text (list, map) do not carry it. */
#include "aprs_core.h"

unsigned aprs_wrap(unsigned y, const char *text, unsigned limit, uint16_t fg)
{
    char b[27];
    unsigned at = 0;
    while (text[at] && y + 8 <= limit) {
        while (text[at] == ' ') ++at;
        if (!text[at]) break;
        unsigned n = 0, space = 0;
        while (n < 26 && text[at + n] && text[at + n] != '\n') {
            if (text[at + n] == ' ') space = n;
            ++n;
        }
        if (n == 26 && text[at + n] && text[at + n] != '\n' && space) n = space;
        unsigned used = n;
        while (n && text[at + n - 1] == ' ') --n;
        for (unsigned i = 0; i < n; ++i) b[i] = text[at + i];
        b[n] = 0;
        aprs_line(y, b, fg, GFX_BLACK);
        at += used;
        while (text[at] == ' ' || text[at] == '\n') ++at;
        y += 10;
    }
    return y;
}
