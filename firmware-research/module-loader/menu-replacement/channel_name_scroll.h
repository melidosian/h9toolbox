#ifndef H9_CHANNEL_NAME_SCROLL_H
#define H9_CHANNEL_NAME_SCROLL_H
#include <stdint.h>
enum { CHANNEL_NAME_CELLS = 8, CHANNEL_NAME_MAX_CELLS = 9 };
unsigned channel_name_scroll_start(uint32_t elapsed, unsigned last);
unsigned channel_name_window_width(const char *label, uint32_t elapsed, unsigned cells, char *text);
#endif
