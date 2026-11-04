#include "h9_aprs_tx_runtime.h"

#include <stddef.h>

void h9_aprs_tx_runtime_bind(H9AprsTxRuntime *runtime,
                             H9AprsTxService *service)
{
    if (runtime == NULL) return;
    runtime->service = service;
    atomic_init(&runtime->rf_ready, false);
}

bool h9_aprs_tx_runtime_submit(H9AprsTxRuntime *runtime,
                               const uint8_t *packet, size_t length)
{
    return runtime != NULL && runtime->service != NULL &&
           atomic_load_explicit(&runtime->rf_ready, memory_order_acquire) &&
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
    atomic_store_explicit(&runtime->rf_ready, rf_ready, memory_order_release);
    if (!rf_ready) return h9_aprs_tx_service_cancel(service);
    return h9_aprs_tx_service_step(service, channel_busy, receive_active,
                                    receive_complete, persistence);
}

bool h9_aprs_tx_runtime_wave_active(const H9AprsTxRuntime *runtime)
{
    H9AprsTxPhase phase;
    if (runtime == NULL || runtime->service == NULL)
        return false;
    phase = (H9AprsTxPhase)atomic_load_explicit(&runtime->service->phase,
                                                 memory_order_acquire);
    /* Keep the ADC callback scheduled for the abort acknowledgement, even
     * when RF readiness has already fallen. */
    if (phase == H9_APRS_TX_ABORTING) return true;
    return atomic_load_explicit(&runtime->rf_ready, memory_order_acquire) &&
           (phase == H9_APRS_TX_STARTING || phase == H9_APRS_TX_ENCODING ||
            phase == H9_APRS_TX_DRAINING);
}

bool h9_aprs_tx_runtime_next_sample(H9AprsTxRuntime *runtime,
                                    uint8_t *sample)
{
    return h9_aprs_tx_runtime_wave_active(runtime) &&
           h9_aprs_tx_service_next_sample(runtime->service, sample);
}
