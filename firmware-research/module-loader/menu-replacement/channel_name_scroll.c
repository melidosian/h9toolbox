#include "channel_name_scroll.h"
#include "channel_labels.h"

unsigned channel_name_scroll_start(uint32_t elapsed, unsigned last)
{
    enum { START_PAUSE = 100, STEP = 23, END_PAUSE = 100 }; /* 100 Hz */
    if (!last) return 0;
    unsigned end = START_PAUSE + STEP * (last - 1);
    unsigned phase = elapsed % (end + END_PAUSE);
    unsigned first = phase < START_PAUSE ? 0 : 1 + (phase - START_PAUSE) / STEP;
    return first < last ? first : last;
}

unsigned channel_name_window_width(const char *label, uint32_t elapsed, unsigned cells, char *text)
{
    if (!cells || cells > CHANNEL_NAME_MAX_CELLS) { text[0] = 0; return 0; }
    unsigned length = 0, first = 0;
    while (length < CHANNEL_LABELS_MAX && label[length]) ++length;
    if (length > cells) first = channel_name_scroll_start(elapsed, length - cells);
    for (unsigned i = 0; i < cells; ++i)
        text[i] = first + i < length ? label[first + i] : ' ';
    text[cells] = 0;
    return first;
}
