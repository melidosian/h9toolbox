#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "channel_name_scroll.h"

int main(void)
{
    char display[CHANNEL_NAME_MAX_CELLS + 1];
    const char *long_name = "0123456789ABCDEFGHIJKLMNOPQRSTUV";
    assert(strlen(long_name) == 32);
    assert(channel_name_window_width(long_name, 0, 8, display) == 0 && !strcmp(display, "01234567"));
    assert(channel_name_window_width(long_name, 99, 8, display) == 0);
    assert(channel_name_window_width(long_name, 100, 8, display) == 1 && !strcmp(display, "12345678"));
    assert(channel_name_window_width(long_name, 123, 8, display) == 2 && !strcmp(display, "23456789"));
    assert(channel_name_window_width(long_name, 629, 8, display) == 24 && !strcmp(display, "OPQRSTUV"));
    assert(channel_name_window_width(long_name, 728, 8, display) == 24);
    assert(channel_name_window_width(long_name, 729, 8, display) == 0 && !strcmp(display, "01234567"));
    assert(channel_name_window_width("SHORT", UINT32_MAX, 8, display) == 0 && !strcmp(display, "SHORT   "));
    assert(channel_name_window_width(long_name, 0, 9, display) == 0 && !strcmp(display, "012345678"));
    assert(channel_name_window_width(long_name, 606, 9, display) == 23 && !strcmp(display, "NOPQRSTUV"));
    assert(channel_name_window_width("NAME", 0, 10, display) == 0 && !display[0]);
    puts("Channel name viewport and scroll timing passed.");
}
