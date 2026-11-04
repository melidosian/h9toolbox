/* Independent 1200-baud Bell 202/AX.25 stimulus. This test deliberately
 * bypasses the replacement TX encoder, oscillator, and packet builder. */
#include "h9_aprs_controller.h"
#include "h9_aprs_tx_gate.h"
#include "h9_main_aprs_receive.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "AFSK check failed at line %d: %s\n", \
            __LINE__, #condition); abort(); \
} } while (0)

typedef struct {
    H9AprsController controller;
    H9MainWireReceiver main_receiver;
    H9MainAx25Decoded decoded;
    uint8_t packet[64];
    size_t packet_length;
    unsigned packets;
    unsigned burst_reports;
    bool burst_active;
    uint16_t adc_value;
    unsigned samples_since_poll;
    unsigned poll_every;
    double phase;
    double tone_hz;
    double sample_rate;
    double sample_time;
    double bit_end;
    double amplitude;
    int dc_bias;
    unsigned noise_amplitude;
    unsigned sample_index;
    unsigned ones;
} AfskFixture;

static uint16_t read_adc(void *context)
{
    AfskFixture *fixture = (AfskFixture *)context;
    return fixture->adc_value;
}

static void receive_serial(void *context, const uint8_t *data, size_t length)
{
    AfskFixture *fixture = (AfskFixture *)context;
    size_t index;
    for (index = 0; index < length; ++index) {
        if (h9_main_wire_feed(&fixture->main_receiver, data[index]) ==
            H9_MAIN_WIRE_FRAME) {
            if (fixture->main_receiver.command == 0x52u) {
                CHECK(fixture->main_receiver.length == 2u);
                CHECK(fixture->main_receiver.payload[0] == 1u);
                CHECK(fixture->main_receiver.payload[1] <= 1u);
                fixture->burst_active =
                    fixture->main_receiver.payload[1] == 1u;
                fixture->burst_reports++;
                h9_main_wire_reset(&fixture->main_receiver);
                continue;
            }
            CHECK(fixture->main_receiver.command == 0u);
            CHECK(fixture->main_receiver.length <= sizeof fixture->packet);
            fixture->packet_length = fixture->main_receiver.length;
            memcpy(fixture->packet, fixture->main_receiver.payload,
                   fixture->packet_length);
            CHECK(h9_main_aprs_packet_from_frame(&fixture->main_receiver,
                                                   &fixture->decoded));
            fixture->packets++;
            h9_main_wire_reset(&fixture->main_receiver);
        }
    }
}

static void send_bit(AfskFixture *fixture, unsigned bit)
{
    if (bit == 0u)
        fixture->tone_hz = fixture->tone_hz == 1200.0 ? 2200.0 : 1200.0;
    fixture->bit_end += 1.0 / 1200.0;
    while (fixture->sample_time + 1e-12 < fixture->bit_end) {
        int noise = (int)((fixture->sample_index++ * 17u) % 13u) - 6;
        int sample = (int)(fixture->amplitude * sin(fixture->phase)) +
                     fixture->dc_bias +
                     noise * (int)fixture->noise_amplitude / 6;
        if (sample < -128) sample = -128;
        if (sample > 127) sample = 127;
        fixture->phase += 6.2831853071795864769 * fixture->tone_hz /
                          fixture->sample_rate;
        fixture->sample_time += 1.0 / fixture->sample_rate;
        fixture->adc_value = (uint16_t)((sample + 128) << 2);
        h9_aprs_controller_adc_tick(&fixture->controller);
        if (fixture->poll_every != 0u &&
            ++fixture->samples_since_poll == fixture->poll_every) {
            h9_aprs_controller_process_received(&fixture->controller);
            fixture->samples_since_poll = 0u;
        }
    }
}

static void send_flag(AfskFixture *fixture)
{
    unsigned bit;
    for (bit = 0; bit < 8u; ++bit)
        send_bit(fixture, (0x7Eu >> bit) & 1u);
}

static void send_data_byte(AfskFixture *fixture, uint8_t byte)
{
    unsigned bit;
    for (bit = 0; bit < 8u; ++bit) {
        unsigned value = (byte >> bit) & 1u;
        send_bit(fixture, value);
        fixture->ones = value != 0u ? fixture->ones + 1u : 0u;
        if (fixture->ones == 5u) {
            send_bit(fixture, 0u);
            fixture->ones = 0u;
        }
    }
}

static void test_at_sample_rate(double sample_rate, bool force_overflow,
                                double amplitude, int dc_bias,
                                unsigned noise_amplitude)
{
    /* AX.25 UI frame: APRS-0 > KD9CRQ-0, >TEST. Its independent X.25
     * FCS is 0x9BFA, transmitted low byte first. */
    static const uint8_t packet[] = {
        0x82u, 0xA0u, 0xA4u, 0xA6u, 0x40u, 0x40u, 0x60u,
        0x96u, 0x88u, 0x72u, 0x86u, 0xA4u, 0xA2u, 0x61u,
        0x03u, 0xF0u, '>', 'T', 'E', 'S', 'T'
    };
    static const uint8_t fcs[] = { 0xFAu, 0x9Bu };
    AfskFixture fixture = { 0 };
    H9AprsHardware hardware = { 0 };
    H9AprsDemodGateState gate_state;
    H9AprsTxGate gate;
    size_t index;
    hardware.context = &fixture;
    hardware.read_adc = read_adc;
    hardware.uart_write = receive_serial;
    h9_aprs_controller_init(&fixture.controller, &hardware);
    h9_main_wire_reset(&fixture.main_receiver);
    fixture.tone_hz = 1200.0;
    fixture.sample_rate = sample_rate;
    fixture.amplitude = amplitude;
    fixture.dc_bias = dc_bias;
    fixture.noise_amplitude = noise_amplitude;
    if (force_overflow) {
        /* The 64-byte ISR queue fills when main-loop polling is withheld.
         * Recovery must discard the partial stream on the consumer side. */
        for (index = 0; index < 100u; ++index) send_flag(&fixture);
        CHECK(atomic_load(&fixture.controller.demod.queue_overflow) > 0u);
        h9_aprs_controller_process_received(&fixture.controller);
        CHECK(h9_aprs_ring_empty(&fixture.controller.demod.receive));
        CHECK(fixture.controller.receive_overflow_seen ==
              atomic_load(&fixture.controller.demod.queue_overflow));
    }
    fixture.poll_every = 32u;
    fixture.samples_since_poll = 0u;
    for (index = 0; index < (force_overflow ? 100u : 50u); ++index)
        send_flag(&fixture);
    gate_state = h9_aprs_demod_gate_state(&fixture.controller.demod);
    CHECK(gate_state.in_frame && gate_state.preamble_seen);
    CHECK(h9_aprs_controller_report_burst_activity(&fixture.controller,
                                                    100u));
    CHECK(fixture.burst_reports == 1u && fixture.burst_active);
    h9_aprs_tx_gate_init(&gate, 0u);
    CHECK(h9_aprs_tx_gate_step(&gate, gate_state.preamble_seen,
                               gate_state.in_frame, false, 255u) ==
          H9_APRS_TX_WAIT);
    CHECK(gate.random_state == 0u);
    for (index = 0; index < sizeof packet; ++index)
        send_data_byte(&fixture, packet[index]);
    for (index = 0; index < sizeof fcs; ++index)
        send_data_byte(&fixture, fcs[index]);
    for (index = 0; index < 10u; ++index) send_flag(&fixture);
    h9_aprs_controller_process_received(&fixture.controller);
    CHECK(fixture.controller.valid_rx_frames == 1u);
    CHECK(fixture.controller.forwarded_rx_frames == 1u);
    CHECK(fixture.packets == 1u);
    CHECK(fixture.packet_length == sizeof packet);
    CHECK(memcmp(fixture.packet, packet, sizeof packet) == 0);
    CHECK(memcmp(fixture.decoded.destination, "APRS", 4u) == 0);
    CHECK(memcmp(fixture.decoded.source, "KD9CRQ", 6u) == 0);
    CHECK(fixture.decoded.control == 3u && fixture.decoded.pid == 0xF0u);
    CHECK(fixture.decoded.information_length == 5u);
    CHECK(memcmp(fixture.decoded.information, ">TEST", 5u) == 0);
    /* A complete decoded packet must release the prospective audio mute
     * after the RF waveform ends, including with a DC offset on the ADC. */
    for (index = 0; index < 200u; ++index) {
        fixture.adc_value = (uint16_t)((128 + dc_bias) << 2);
        h9_aprs_controller_adc_tick(&fixture.controller);
    }
    gate_state = h9_aprs_demod_gate_state(&fixture.controller.demod);
    CHECK(!gate_state.in_frame && !gate_state.preamble_seen);
    CHECK(h9_aprs_controller_report_burst_activity(&fixture.controller,
                                                    200u));
    CHECK(fixture.burst_reports == 2u && !fixture.burst_active);
}

int main(void)
{
    test_at_sample_rate(9600.0, false, 95.0, 0, 0u);
    test_at_sample_rate(9600.0, true, 95.0, 0, 0u);
    /* 24 MHz / (11+1) / (209+1) from the recovered timer registers. */
    test_at_sample_rate(24000000.0 / 12.0 / 210.0, false,
                        95.0, 0, 0u);
    test_at_sample_rate(24000000.0 / 12.0 / 210.0, true,
                        95.0, 0, 0u);
    test_at_sample_rate(24000000.0 / 12.0 / 210.0, false,
                        65.0, 18, 4u);
    return 0;
}
