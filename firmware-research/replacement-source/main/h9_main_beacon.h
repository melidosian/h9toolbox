#ifndef H9_MAIN_BEACON_H
#define H9_MAIN_BEACON_H

#include <stdbool.h>
#include <stdint.h>

/* Port of main app+0x082EB2. The outer caller's receive/busy guards and the
   tick period have not yet been recovered. */
typedef struct { uint32_t counter; } H9BeaconScheduler;
typedef struct {
    bool call_tx_selector;
    bool restore_receive_if_selector_succeeds;
} H9BeaconDecision;

H9BeaconDecision h9_main_beacon_tick(H9BeaconScheduler *scheduler,
                                     uint8_t timed_beacon_setting,
                                     uint16_t interval_setting,
                                     uint8_t state_11b1e,
                                     uint8_t ptt_priority);

#endif
