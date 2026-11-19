#ifndef H9_BEACON_STATE_H
#define H9_BEACON_STATE_H

/* Loader API 3 clears both halves at boot. The module overlay starts at 0x29F00. */
#ifndef BEACON_BACKGROUND_STATE
#define BEACON_BACKGROUND_STATE ((struct screen *)0x29C00u)
#endif
#ifndef BEACON_HANDOFF
#define BEACON_HANDOFF ((volatile uint8_t *)0x29D80u)
#endif

#endif
