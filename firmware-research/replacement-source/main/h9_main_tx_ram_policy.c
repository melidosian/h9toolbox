#include "h9_main_tx_ram_policy.h"

#include <stddef.h>
#include <stdint.h>

void h9_main_tx_ram_policy_bind_target(H9MainTxRamPolicy *memory)
{
    if (memory == NULL) return;
    memory->position_source = (const volatile uint8_t *)(uintptr_t)0xC1CAu;
    memory->gps_fix_valid = (const volatile uint8_t *)(uintptr_t)0x12550u;
    memory->latitude_raw = (const volatile uint32_t *)(uintptr_t)0x475C8u;
    memory->longitude_raw = (const volatile uint32_t *)(uintptr_t)0x475D0u;
    memory->region_mode = (const volatile uint8_t *)(uintptr_t)0x12B9Cu;
    memory->allow_200_to_300_mhz =
        (const volatile uint8_t *)(uintptr_t)0x12BDCu;
    memory->allow_300_to_400_mhz =
        (const volatile uint8_t *)(uintptr_t)0x12BDDu;
    memory->allow_500_mhz_plus =
        (const volatile uint8_t *)(uintptr_t)0x12BDEu;
    memory->range0_low_10hz = (const volatile uint32_t *)(uintptr_t)0xC120u;
    memory->range0_high_10hz = (const volatile uint32_t *)(uintptr_t)0xC124u;
    memory->range1_low_10hz = (const volatile uint32_t *)(uintptr_t)0xC118u;
    memory->range1_high_10hz = (const volatile uint32_t *)(uintptr_t)0xC11Cu;
}

bool h9_main_tx_ram_policy_read_position(const H9MainTxRamPolicy *memory,
                                          H9BeaconPositionStatus *status)
{
    H9BeaconPositionStatus value;
    if (memory == NULL || status == NULL ||
        memory->position_source == NULL || memory->gps_fix_valid == NULL ||
        memory->latitude_raw == NULL || memory->longitude_raw == NULL)
        return false;
    value.position_source = *memory->position_source;
    value.gps_fix_valid = *memory->gps_fix_valid != 0u;
    value.latitude_raw = *memory->latitude_raw;
    value.longitude_raw = *memory->longitude_raw;
    if (value.position_source != *memory->position_source ||
        value.gps_fix_valid != (*memory->gps_fix_valid != 0u) ||
        value.latitude_raw != *memory->latitude_raw ||
        value.longitude_raw != *memory->longitude_raw) return false;
    *status = value;
    return true;
}

bool h9_main_tx_ram_policy_frequency_allowed(
    const H9MainTxRamPolicy *memory, uint32_t tx_hz)
{
    H9MainTxFrequencyPolicy policy;
    H9MainTxLimits limits;
    if (memory == NULL || memory->region_mode == NULL ||
        memory->allow_200_to_300_mhz == NULL ||
        memory->allow_300_to_400_mhz == NULL ||
        memory->allow_500_mhz_plus == NULL ||
        memory->range0_low_10hz == NULL ||
        memory->range0_high_10hz == NULL ||
        memory->range1_low_10hz == NULL ||
        memory->range1_high_10hz == NULL) return false;
    policy.region_mode = *memory->region_mode;
    policy.allow_200_to_300_mhz = *memory->allow_200_to_300_mhz != 0u;
    policy.allow_300_to_400_mhz = *memory->allow_300_to_400_mhz != 0u;
    policy.allow_500_mhz_plus = *memory->allow_500_mhz_plus != 0u;
    limits.range0_low_10hz = *memory->range0_low_10hz;
    limits.range0_high_10hz = *memory->range0_high_10hz;
    limits.range1_low_10hz = *memory->range1_low_10hz;
    limits.range1_high_10hz = *memory->range1_high_10hz;
    if (policy.region_mode != *memory->region_mode ||
        policy.allow_200_to_300_mhz !=
            (*memory->allow_200_to_300_mhz != 0u) ||
        policy.allow_300_to_400_mhz !=
            (*memory->allow_300_to_400_mhz != 0u) ||
        policy.allow_500_mhz_plus !=
            (*memory->allow_500_mhz_plus != 0u) ||
        limits.range0_low_10hz != *memory->range0_low_10hz ||
        limits.range0_high_10hz != *memory->range0_high_10hz ||
        limits.range1_low_10hz != *memory->range1_low_10hz ||
        limits.range1_high_10hz != *memory->range1_high_10hz) return false;
    return h9_main_tx_hz_allowed_with_limits(&policy, &limits, tx_hz);
}
