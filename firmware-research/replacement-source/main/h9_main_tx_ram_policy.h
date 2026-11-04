#ifndef H9_MAIN_TX_RAM_POLICY_H
#define H9_MAIN_TX_RAM_POLICY_H

#include "h9_main_beacon.h"
#include "h9_main_tx_frequency.h"

/* Stock main RAM fields consumed by APRS position preflight and the TX
 * frequency gate. Binding reads nothing and does not change RF state. */
typedef struct {
    const volatile uint8_t *position_source;
    const volatile uint8_t *gps_fix_valid;
    const volatile uint32_t *latitude_raw;
    const volatile uint32_t *longitude_raw;
    const volatile uint8_t *region_mode;
    const volatile uint8_t *allow_200_to_300_mhz;
    const volatile uint8_t *allow_300_to_400_mhz;
    const volatile uint8_t *allow_500_mhz_plus;
    const volatile uint32_t *range0_low_10hz;
    const volatile uint32_t *range0_high_10hz;
    const volatile uint32_t *range1_low_10hz;
    const volatile uint32_t *range1_high_10hz;
} H9MainTxRamPolicy;

void h9_main_tx_ram_policy_bind_target(H9MainTxRamPolicy *memory);
bool h9_main_tx_ram_policy_read_position(const H9MainTxRamPolicy *memory,
                                          H9BeaconPositionStatus *status);
bool h9_main_tx_ram_policy_frequency_allowed(
    const H9MainTxRamPolicy *memory, uint32_t tx_hz);

#endif
