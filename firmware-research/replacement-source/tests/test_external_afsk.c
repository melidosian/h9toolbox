/* Independent 1200-baud Bell 202/AX.25 stimulus. This test deliberately
 * bypasses the replacement TX encoder, oscillator, and packet builder. */
#include "h9_aprs_demod.h"
#include "h9_aprs_frame.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) abort(); } while (0)

typedef struct {
    H9AprsDemod demod;
    H9AprsFrame frame;
    uint8_t packet[64];
    size_t packet_length;
    unsigned packets;
    double phase;
    double tone_hz;
    double sample_rate;
    double sample_time;
    double bit_end;
    unsigned ones;
} AfskFixture;

static void capture_packet(void *context, const uint8_t *data, size_t length)
{
    AfskFixture *fixture = (AfskFixture *)context;
    CHECK(length <= sizeof fixture->packet);
    memcpy(fixture->packet, data, length);
    fixture->packet_length = length;
    fixture->packets++;
}

static void send_bit(AfskFixture *fixture, unsigned bit)
{
    uint8_t byte;
    if (bit == 0u)
        fixture->tone_hz = fixture->tone_hz == 1200.0 ? 2200.0 : 1200.0;
    fixture->bit_end += 1.0 / 1200.0;
    while (fixture->sample_time + 1e-12 < fixture->bit_end) {
        int16_t sample = (int16_t)(95.0 * sin(fixture->phase));
        fixture->phase += 6.2831853071795864769 * fixture->tone_hz /
                          fixture->sample_rate;
        fixture->sample_time += 1.0 / fixture->sample_rate;
        CHECK(h9_aprs_demod_sample(&fixture->demod, sample));
        while (h9_aprs_demod_pop_byte(&fixture->demod, &byte))
            (void)h9_aprs_frame_feed(&fixture->frame, byte,
                                      capture_packet, fixture);
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

static void test_at_sample_rate(double sample_rate)
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
    size_t index;
    h9_aprs_demod_init(&fixture.demod);
    h9_aprs_frame_init(&fixture.frame);
    fixture.tone_hz = 1200.0;
    fixture.sample_rate = sample_rate;
    for (index = 0; index < 50u; ++index) send_flag(&fixture);
    for (index = 0; index < sizeof packet; ++index)
        send_data_byte(&fixture, packet[index]);
    for (index = 0; index < sizeof fcs; ++index)
        send_data_byte(&fixture, fcs[index]);
    for (index = 0; index < 10u; ++index) send_flag(&fixture);
    CHECK(fixture.packets == 1u);
    CHECK(fixture.packet_length == sizeof packet);
    CHECK(memcmp(fixture.packet, packet, sizeof packet) == 0);
}

int main(void)
{
    test_at_sample_rate(9600.0);
    /* 24 MHz / (11+1) / (209+1) from the recovered timer registers. */
    test_at_sample_rate(24000000.0 / 12.0 / 210.0);
    return 0;
}
