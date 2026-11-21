#include "channel_name_scroll.h"
#include "channel_labels.h"

unsigned channel_name_window_width(const char *label, uint32_t elapsed, unsigned cells, char *text)
{
    enum { START_PAUSE = 100, STEP = 23, END_PAUSE = 100 }; /* 100 Hz */
    if (!cells || cells > CHANNEL_NAME_MAX_CELLS) { text[0] = 0; return 0; }
    unsigned length = 0, first = 0;
    while (length < CHANNEL_LABELS_MAX && label[length]) ++length;
    if (length > cells) {
        unsigned last = length - cells;
        unsigned end = START_PAUSE + STEP * (last - 1);
        unsigned phase = elapsed % (end + END_PAUSE);
        if (phase >= START_PAUSE) first = 1 + (phase - START_PAUSE) / STEP;
        if (first > last) first = last;
    }
    for (unsigned i = 0; i < cells; ++i)
        text[i] = first + i < length ? label[first + i] : ' ';
    text[cells] = 0;
    return first;
}
