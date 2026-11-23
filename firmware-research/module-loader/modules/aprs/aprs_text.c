/* aprs_wrap (aprs_core.h), apart from the core so screens that do not wrap
 * text (list, map) do not carry it. */
#include "aprs_core.h"

unsigned aprs_wrap(unsigned y, const char *text, unsigned limit, uint16_t fg)
{
    unsigned at = 0;
    while (text[at] && y + 8 <= limit) {
        while (text[at] == ' ' || text[at] == '\n') ++at;
        if (!text[at]) break;
        unsigned n = gfx_label_fit(text + at, H9_WIDTH - 4);
        if (text[at + n] && text[at + n] != '\n' && text[at + n] != ' ') {
            unsigned word = n;
            while (word && text[at + word] != ' ') --word;
            if (word) n = word;
        }
        char line[79]; /* two pixels is the narrowest advance */
        __builtin_memcpy(line, text + at, n);
        line[n] = 0;
        aprs_line(y, line, fg, GFX_BLACK);
        at += n;
        y += 10;
    }
    return y;
}
