#include "h9_aprs_tx_service.h"
#include <string.h>

static void refill(H9AprsTxService *service)
{
    if (service->phase != H9_APRS_TX_ENCODING) return;
    (void)h9_aprs_tx_packet_pump(&service->encoder, &service->wave);
    if (h9_aprs_tx_packet_done(&service->encoder)) {
        service->phase = H9_APRS_TX_DRAINING;
        service->encoded_event_pending = true;
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
    service->phase = H9_APRS_TX_IDLE;
}

bool h9_aprs_tx_service_submit(H9AprsTxService *service,
                                const uint8_t *packet, size_t length)
{
    if (service == NULL || packet == NULL || length == 0 ||
        length > sizeof service->packet || service->phase != H9_APRS_TX_IDLE ||
        service->wave.active) return false;
    memcpy(service->packet, packet, length);
    service->packet_length = length;
    service->phase = H9_APRS_TX_WAITING;
    return true;
}

H9AprsTxPhase h9_aprs_tx_service_step(H9AprsTxService *service,
                                       bool channel_busy,
                                       bool receive_active,
                                       bool receive_complete,
                                       uint8_t persistence)
{
    H9AprsTxGateResult gate_result;
    if (service == NULL) return H9_APRS_TX_IDLE;
    if (service->phase == H9_APRS_TX_WAITING) {
        gate_result = h9_aprs_tx_gate_step(&service->gate, channel_busy,
                                           receive_active, receive_complete,
                                           persistence);
        if (gate_result == H9_APRS_TX_DROP) {
            service->packet_length = 0;
            service->phase = H9_APRS_TX_IDLE;
        } else if (gate_result == H9_APRS_TX_SEND) {
            if (!h9_aprs_tx_packet_init(&service->encoder, service->packet,
                                         service->packet_length)) {
                service->phase = H9_APRS_TX_IDLE;
            } else {
                service->phase = H9_APRS_TX_ENCODING;
            }
        }
    }
    refill(service);
    if (service->phase == H9_APRS_TX_DRAINING && !service->wave.active) {
        service->packet_length = 0;
        service->phase = H9_APRS_TX_IDLE;
    }
    return service->phase;
}

bool h9_aprs_tx_service_next_sample(H9AprsTxService *service,
                                     uint8_t *sample)
{
    if (service == NULL || sample == NULL ||
        (service->phase != H9_APRS_TX_ENCODING &&
         service->phase != H9_APRS_TX_DRAINING)) return false;
    /* Refill before every sample so a small queue cannot end the waveform
       early while the rest of a long packet is still being encoded. */
    refill(service);
    if (h9_aprs_tx_next_sample(&service->wave, sample)) return true;
    if (service->phase == H9_APRS_TX_DRAINING) {
        service->packet_length = 0;
        service->phase = H9_APRS_TX_IDLE;
    }
    return false;
}

bool h9_aprs_tx_service_take_encoded_event(H9AprsTxService *service)
{
    bool pending;
    if (service == NULL) return false;
    pending = service->encoded_event_pending;
    service->encoded_event_pending = false;
    return pending;
}
