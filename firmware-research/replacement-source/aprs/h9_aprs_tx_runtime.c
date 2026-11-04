#include "h9_aprs_tx_runtime.h"

#include <stddef.h>

static void abort_wave(H9AprsTxService *service)
{
    uint32_t random_state = service->gate.random_state;
    uint16_t lead_ms = service->wave.lead_ms;
    uint16_t tail_ms = service->wave.tail_ms;
    h9_aprs_tx_service_init(service, random_state, lead_ms, tail_ms);
}

void h9_aprs_tx_runtime_bind(H9AprsTxRuntime *runtime,
                             H9AprsTxService *service)
{
    if (runtime == NULL) return;
    runtime->service = service;
    runtime->rf_ready = false;
}

bool h9_aprs_tx_runtime_submit(H9AprsTxRuntime *runtime,
                               const uint8_t *packet, size_t length)
{
    return runtime != NULL && runtime->service != NULL &&
           runtime->rf_ready &&
           h9_aprs_tx_service_submit(runtime->service, packet, length);
}

H9AprsTxPhase h9_aprs_tx_runtime_step(H9AprsTxRuntime *runtime,
                                      bool rf_ready, bool channel_busy,
                                      bool receive_active,
                                      bool receive_complete,
                                      uint8_t persistence)
{
    H9AprsTxService *service;
    if (runtime == NULL || runtime->service == NULL) return H9_APRS_TX_IDLE;
    service = runtime->service;
    runtime->rf_ready = rf_ready;
    if (!rf_ready) {
        if (service->phase != H9_APRS_TX_IDLE)
            abort_wave(service);
        return service->phase;
    }
    return h9_aprs_tx_service_step(service, channel_busy, receive_active,
                                    receive_complete, persistence);
}

bool h9_aprs_tx_runtime_wave_active(const H9AprsTxRuntime *runtime)
{
    if (runtime == NULL || runtime->service == NULL || !runtime->rf_ready)
        return false;
    return (runtime->service->phase == H9_APRS_TX_ENCODING ||
            runtime->service->phase == H9_APRS_TX_DRAINING) &&
           runtime->service->wave.active != 0u;
}

bool h9_aprs_tx_runtime_next_sample(H9AprsTxRuntime *runtime,
                                    uint8_t *sample)
{
    return h9_aprs_tx_runtime_wave_active(runtime) &&
           h9_aprs_tx_service_next_sample(runtime->service, sample);
}
