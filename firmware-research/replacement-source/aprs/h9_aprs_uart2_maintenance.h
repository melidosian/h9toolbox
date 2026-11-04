#ifndef H9_APRS_UART2_MAINTENANCE_H
#define H9_APRS_UART2_MAINTENANCE_H

#include <stdbool.h>
#include <stdint.h>

/* The stock main-loop tail checks recovery every 1,000 passes and after
 * more than 2,000 SysTick units. This replacement uses its latched TX fault
 * and idle RX state in place of the unmapped stock descriptor flags. */
typedef struct {
    uint32_t loop_passes;
    uint32_t last_attempt_tick;
} H9AprsUart2Maintenance;

void h9_aprs_uart2_maintenance_init(H9AprsUart2Maintenance *state);
bool h9_aprs_uart2_maintenance_due(H9AprsUart2Maintenance *state,
                                    uint32_t now_tick, bool faulted,
                                    bool receive_idle);

#endif
