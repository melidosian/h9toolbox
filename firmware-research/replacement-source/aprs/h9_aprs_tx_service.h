#ifndef H9_APRS_TX_SERVICE_H
#define H9_APRS_TX_SERVICE_H

#include "h9_aprs_tx_gate.h"
#include "h9_aprs_tx_packet.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Owns a serial TX request until the gate permits encoding and the final
   waveform sample has drained. The board must manage PTT and DAC timing. */
typedef enum {
    H9_APRS_TX_IDLE,
    H9_APRS_TX_WAITING,
    H9_APRS_TX_STARTING,
    H9_APRS_TX_ENCODING,
    H9_APRS_TX_DRAINING,
    H9_APRS_TX_ABORTING
} H9AprsTxPhase;

typedef struct {
    H9AprsTxGate gate;
    H9AprsTxPacket encoder;
    H9AprsTx wave;
    uint8_t packet[H9_APRS_MAX_RECEIVE];
    size_t packet_length;
    atomic_int phase;
    atomic_uint_least32_t encoded_events;
    uint32_t reported_events;
    atomic_uint_least32_t pending_timing_ms;
} H9AprsTxService;

void h9_aprs_tx_service_init(H9AprsTxService *service,
                              uint32_t random_seed,
                              uint16_t lead_ms, uint16_t tail_ms);
bool h9_aprs_tx_service_submit(H9AprsTxService *service,
                                const uint8_t *packet, size_t length);
void h9_aprs_tx_service_set_timing(H9AprsTxService *service,
                                    uint16_t lead_ms, uint16_t tail_ms);
H9AprsTxPhase h9_aprs_tx_service_cancel(H9AprsTxService *service);
H9AprsTxPhase h9_aprs_tx_service_step(H9AprsTxService *service,
                                       bool channel_busy,
                                       bool receive_active,
                                       bool receive_complete,
                                       uint8_t persistence);
bool h9_aprs_tx_service_next_sample(H9AprsTxService *service,
                                     uint8_t *sample);
/* One event when the closing flag has entered the waveform queue. */
bool h9_aprs_tx_service_take_encoded_event(H9AprsTxService *service);

#endif
