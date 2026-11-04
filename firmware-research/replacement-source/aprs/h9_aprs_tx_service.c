#include "h9_aprs_tx_service.h"
#include <string.h>

static H9AprsTxPhase phase_now(const H9AprsTxService *service)
{
    return (H9AprsTxPhase)atomic_load_explicit(&service->phase,
                                                memory_order_acquire);
}

static uint32_t pack_timing(uint16_t lead_ms, uint16_t tail_ms)
{
    return ((uint32_t)tail_ms << 16) | lead_ms;
}

/* The ADC interrupt alone owns encoder and wave. Main-loop cancellation is
 * acknowledged here before another packet may reuse the buffer. */
static void finish_abort(H9AprsTxService *service)
{
    uint32_t timing = atomic_load_explicit(&service->pending_timing_ms,
                                           memory_order_acquire);
    h9_aprs_tx_init(&service->wave, (uint16_t)timing,
                    (uint16_t)(timing >> 16));
    service->packet_length = 0u;
    atomic_store_explicit(&service->phase, H9_APRS_TX_IDLE,
                          memory_order_release);
}

static void refill(H9AprsTxService *service)
{
    if (phase_now(service) != H9_APRS_TX_ENCODING) return;
    (void)h9_aprs_tx_packet_pump(&service->encoder, &service->wave);
    if (!h9_aprs_tx_packet_done(&service->encoder)) return;
    /* Main cannot preempt this interrupt; a fresh phase read is sufficient. */
    if (phase_now(service) == H9_APRS_TX_ENCODING) {
        atomic_store_explicit(&service->phase, H9_APRS_TX_DRAINING,
                              memory_order_release);
        atomic_store_explicit(&service->encoded_events,
            atomic_load_explicit(&service->encoded_events,
                                 memory_order_relaxed) + 1u,
            memory_order_release);
    }
}

void h9_aprs_tx_service_init(H9AprsTxService *service,
                              uint32_t random_seed,
                              uint16_t lead_ms, uint16_t tail_ms)
{
    if (service == NULL) return;
    memset(service, 0, sizeof *service);
    h9_aprs_tx_gate_init(&service->gate, random_seed);
    h9_aprs_tx_init(&service->wave, lead_ms, tail_ms);
    atomic_init(&service->phase, H9_APRS_TX_IDLE);
    atomic_init(&service->encoded_events, 0u);
    atomic_init(&service->pending_timing_ms, pack_timing(lead_ms, tail_ms));
}

void h9_aprs_tx_service_set_timing(H9AprsTxService *service,
                                    uint16_t lead_ms, uint16_t tail_ms)
{
    if (service == NULL) return;
    atomic_store_explicit(&service->pending_timing_ms,
                          pack_timing(lead_ms, tail_ms),
                          memory_order_release);
}

bool h9_aprs_tx_service_submit(H9AprsTxService *service,
                                const uint8_t *packet, size_t length)
{
    if (service == NULL || packet == NULL || length == 0u ||
        length > sizeof service->packet ||
        phase_now(service) != H9_APRS_TX_IDLE) return false;
    memcpy(service->packet, packet, length);
    service->packet_length = length;
    atomic_store_explicit(&service->phase, H9_APRS_TX_WAITING,
                          memory_order_release);
    return true;
}

H9AprsTxPhase h9_aprs_tx_service_cancel(H9AprsTxService *service)
{
    H9AprsTxPhase phase;
    if (service == NULL) return H9_APRS_TX_IDLE;
    phase = phase_now(service);
    if (phase == H9_APRS_TX_IDLE || phase == H9_APRS_TX_ABORTING)
        return phase;
    if (phase == H9_APRS_TX_WAITING) {
        service->packet_length = 0u;
        atomic_store_explicit(&service->phase, H9_APRS_TX_IDLE,
                              memory_order_release);
        return H9_APRS_TX_IDLE;
    }
    atomic_store_explicit(&service->phase, H9_APRS_TX_ABORTING,
                          memory_order_release);
    return H9_APRS_TX_ABORTING;
}

H9AprsTxPhase h9_aprs_tx_service_step(H9AprsTxService *service,
                                       bool preamble_seen,
                                       bool in_frame,
                                       bool queue_error,
                                       uint8_t persistence)
{
    H9AprsTxGateResult result;
    if (service == NULL) return H9_APRS_TX_IDLE;
    if (phase_now(service) == H9_APRS_TX_WAITING) {
        result = h9_aprs_tx_gate_step(&service->gate, preamble_seen,
                                      in_frame, queue_error,
                                      persistence);
        if (result == H9_APRS_TX_DROP) {
            service->packet_length = 0u;
            atomic_store_explicit(&service->phase, H9_APRS_TX_IDLE,
                                  memory_order_release);
        } else if (result == H9_APRS_TX_SEND) {
            /* Publish the copied packet; the ADC interrupt initializes and
             * advances the encoder and waveform after this handoff. */
            atomic_store_explicit(&service->phase, H9_APRS_TX_STARTING,
                                  memory_order_release);
        }
    }
    return phase_now(service);
}

bool h9_aprs_tx_service_next_sample(H9AprsTxService *service,
                                     uint8_t *sample)
{
    H9AprsTxPhase phase;
    if (service == NULL || sample == NULL) return false;
    phase = phase_now(service);
    if (phase == H9_APRS_TX_ABORTING) {
        finish_abort(service);
        return false;
    }
    if (phase == H9_APRS_TX_STARTING) {
        uint32_t timing = atomic_load_explicit(&service->pending_timing_ms,
                                               memory_order_acquire);
        h9_aprs_tx_init(&service->wave, (uint16_t)timing,
                        (uint16_t)(timing >> 16));
        if (!h9_aprs_tx_packet_init(&service->encoder, service->packet,
                                     service->packet_length)) {
            atomic_store_explicit(&service->phase, H9_APRS_TX_IDLE,
                                  memory_order_release);
            return false;
        }
        atomic_store_explicit(&service->phase, H9_APRS_TX_ENCODING,
                              memory_order_release);
    } else if (phase != H9_APRS_TX_ENCODING &&
               phase != H9_APRS_TX_DRAINING) {
        return false;
    }
    /* Refill before each sample so a 64-byte queue cannot run dry during a
     * long packet. Only this interrupt writes encoder and wave state. */
    refill(service);
    if (phase_now(service) == H9_APRS_TX_ABORTING) {
        finish_abort(service);
        return false;
    }
    if (h9_aprs_tx_next_sample(&service->wave, sample)) {
        if (phase_now(service) != H9_APRS_TX_ABORTING) return true;
        finish_abort(service);
        return false;
    }
    if (phase_now(service) == H9_APRS_TX_DRAINING)
        atomic_store_explicit(&service->phase, H9_APRS_TX_IDLE,
                              memory_order_release);
    else if (phase_now(service) == H9_APRS_TX_ABORTING)
        finish_abort(service);
    return false;
}

bool h9_aprs_tx_service_take_encoded_event(H9AprsTxService *service)
{
    uint32_t events;
    if (service == NULL) return false;
    events = atomic_load_explicit(&service->encoded_events,
                                  memory_order_acquire);
    if (events == service->reported_events) return false;
    service->reported_events = events;
    return true;
}
