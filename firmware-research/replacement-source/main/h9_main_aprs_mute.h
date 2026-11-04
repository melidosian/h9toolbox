#ifndef H9_MAIN_APRS_MUTE_H
#define H9_MAIN_APRS_MUTE_H

#include <stdbool.h>
#include <stdint.h>

#include "h9_main_aprs.h"

typedef struct {
    uint32_t last_active_ms;
    bool active;
} H9MainAprsMuteLease;

/* Update from the replacement APRS command 0x52 callback. The board owns
 * the clock and must use the same millisecond source for query and update. */
void h9_main_aprs_mute_event(H9MainAprsMuteLease *lease,
                              bool active, uint32_t now_ms);

/* One decision for both physical audible paths. Route verification must
 * establish that the current received audio belongs to the APRS channel.
 * The board output gates have not yet been identified or connected. */
bool h9_main_aprs_mute_should_silence(const H9MainAprsMuteLease *lease,
                                      bool mute_option_enabled,
                                      bool aprs_receive_route_active,
                                      uint32_t now_ms);

/* Compose the replacement burst lease with the existing APRS audio policy.
 * route_verified must come from the live receive-audio route, not merely
 * the selected display line or the TX route snapshot. */
H9AudioPlan h9_main_aprs_mute_plan(const H9MainAprsConfig *config,
                                   const H9MainAprsMuteLease *lease,
                                   H9Side received_side,
                                   bool route_verified,
                                   uint32_t now_ms);

/* Replacement audio-route boundary. The adapter must hold its ordinary
 * receive-route state from begin through end, without masking interrupts
 * for the duration. read_route returns the ordinary output requests before
 * APRS mute is overlaid, not the already-gated output levels. Both physical
 * setters are required: the local speaker pin alone cannot mute Bluetooth.
 * A failed setter can leave one output changed; the adapter must reconcile
 * the route before returning to normal audio service. */
typedef struct {
    bool (*begin_route)(void *context);
    bool (*read_route)(void *context, H9Side *received_side,
                       bool *route_verified, bool *speaker_requested,
                       bool *bluetooth_requested);
    bool (*set_speaker)(void *context, bool enabled);
    bool (*set_bluetooth)(void *context, bool enabled);
    bool (*end_route)(void *context);
    void *context;
} H9MainAprsMuteOutputs;

bool h9_main_aprs_mute_apply(const H9MainAprsConfig *config,
                             const H9MainAprsMuteLease *lease,
                             uint32_t now_ms,
                             const H9MainAprsMuteOutputs *outputs);

#endif
