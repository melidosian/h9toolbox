#ifndef H9_APRS_TX_PERMIT_H
#define H9_APRS_TX_PERMIT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t renewed_ms;
    uint16_t token;
    bool enabled;
} H9AprsTxPermit;

void h9_aprs_tx_permit_set(H9AprsTxPermit *permit, bool enabled,
                            uint16_t token, uint32_t now_ms);
bool h9_aprs_tx_permit_active(const H9AprsTxPermit *permit,
                              uint32_t now_ms);

#endif
