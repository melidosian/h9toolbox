#ifndef H9_APRS_TX_RUNTIME_H
#define H9_APRS_TX_RUNTIME_H

#include "h9_aprs_tx_service.h"

/* Connects serial packet requests to the waveform service. The caller must
 * establish RF readiness from a board signal before each cooperative step.
 * No stock GPIO has yet been proven to be that signal. */
typedef struct {
    H9AprsTxService *service;
    bool rf_ready;
} H9AprsTxRuntime;

void h9_aprs_tx_runtime_bind(H9AprsTxRuntime *runtime,
                             H9AprsTxService *service);
bool h9_aprs_tx_runtime_submit(H9AprsTxRuntime *runtime,
                               const uint8_t *packet, size_t length);
H9AprsTxPhase h9_aprs_tx_runtime_step(H9AprsTxRuntime *runtime,
                                      bool rf_ready, bool channel_busy,
                                      bool receive_active,
                                      bool receive_complete,
                                      uint8_t persistence);
bool h9_aprs_tx_runtime_wave_active(const H9AprsTxRuntime *runtime);
bool h9_aprs_tx_runtime_next_sample(H9AprsTxRuntime *runtime,
                                    uint8_t *sample);

#endif
