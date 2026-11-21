#ifndef H9_RESIDENT_BEACON_TX_H
#define H9_RESIDENT_BEACON_TX_H

#include "runtime.h"

#define H9_RF_SLEEP_FLAG 0x11B01u

struct h9_beacon_attempt;
struct h9_beacon_tx {
    struct h9_beacon_attempt *attempt;
    volatile uint16_t stock_calls;
    volatile uint8_t active;
    uint8_t fault;
};

typedef void (*h9_stock_action)(const void *, unsigned);
enum { H9_TX_SUBMITTED = 0 };

/* Called on my_app, at the idle call site of the stock timed beacon. */
unsigned h9_beacon_tx_try(struct h9_beacon_tx *, const struct h9_runtime_status *);

/* Hooks inside the stock beacon path. They pass through to the original
 * routines while the resident service has no active attempt. */
unsigned h9_beacon_tx_prepare(struct h9_beacon_tx *, const void *frequency);
unsigned h9_beacon_tx_emit(struct h9_beacon_tx *);
unsigned h9_beacon_tx_airtime(struct h9_beacon_tx *);

/* Stock AF01 KISS and remote PTT tasks share ownership with automatic TX. */
void h9_beacon_tx_stock(struct h9_beacon_tx *, const void *, unsigned, h9_stock_action);

#endif
