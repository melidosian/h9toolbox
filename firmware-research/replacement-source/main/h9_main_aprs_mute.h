#ifndef H9_MAIN_APRS_MUTE_H
#define H9_MAIN_APRS_MUTE_H

#include <stdbool.h>
#include <stdint.h>

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

#endif
