#ifndef H9_RADIO_FOCUS_H
#define H9_RADIO_FOCUS_H

#define RADIO_DISPLAY_LAYOUT_OFFSET 0x1F29u
enum {
    RADIO_DISPLAY_SINGLE, RADIO_DISPLAY_DUAL, RADIO_DISPLAY_CLASSIC, RADIO_DISPLAY_FOCUS
};

int radio_focus_enabled(void);
/* Call from the radio's normal home loop, after native settings load. */
void radio_focus_sync(void);
int radio_focus_reset(void);

#endif
