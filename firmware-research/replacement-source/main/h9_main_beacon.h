#ifndef H9_MAIN_BEACON_H
#define H9_MAIN_BEACON_H

#include <stdbool.h>
#include <stdint.h>

/* Port of main app+0x082EB2. Its caller gate at app+0x08747C is modeled
 * below. Two top-level mode loops call the caller once and then yield for
 * one ROM scheduler tick; that tick's duration in milliseconds is unknown. */
typedef struct { uint32_t counter; } H9BeaconScheduler;
typedef struct {
    bool call_tx_selector;
    bool restore_receive_if_selector_succeeds;
} H9BeaconDecision;

/* Raw state bytes read by the stock caller. Their names deliberately retain
 * RAM addresses until the mode and receive flags are identified. */
typedef struct {
    uint8_t state_11ab0;
    uint8_t state_11ac6;
    uint8_t flag_11afb;
    uint8_t flag_11afe;
} H9BeaconCallerState;

bool h9_main_beacon_caller_allows_tick(const H9BeaconCallerState *state);
H9BeaconDecision h9_main_beacon_caller_tick(
    H9BeaconScheduler *scheduler, const H9BeaconCallerState *state,
    uint8_t timed_beacon_setting, uint16_t interval_setting,
    uint8_t state_11b1e, uint8_t ptt_priority);

/* Stock selector app+0x05C2D4 requires a usable GPS position only when its
 * source byte at 0xC1CA equals 1. The two coordinate words are the stock
 * raw fields at 0x475C8 and 0x475D0; their units are immaterial to this
 * nonzero gate. */
typedef struct {
    uint8_t position_source;
    bool gps_fix_valid;
    uint32_t latitude_raw;
    uint32_t longitude_raw;
} H9BeaconPositionStatus;

bool h9_main_beacon_position_ready(const H9BeaconPositionStatus *status);

H9BeaconDecision h9_main_beacon_tick(H9BeaconScheduler *scheduler,
                                     uint8_t timed_beacon_setting,
                                     uint16_t interval_setting,
                                     uint8_t state_11b1e,
                                     uint8_t ptt_priority);

#endif
