#include "h9_aprs_controller.h"
#include "h9_aprs_bitstream.h"
#include "h9_aprs_ring.h"
#include "h9_main_aprs.h"
#include <assert.h>
#include <string.h>

typedef struct {
    uint8_t uart[H9_APRS_ENCODED_CAPACITY];
    size_t uart_length;
    uint8_t packet[H9_APRS_MAX_RECEIVE];
    size_t packet_length;
    int demod_calls;
    int dac_calls;
    int16_t sample;
    uint8_t dac;
    bool tx_active;
} FakeHardware;

static void uart_write(void *opaque, const uint8_t *data, size_t length)
{
    FakeHardware *fake = (FakeHardware *)opaque;
    assert(length <= sizeof fake->uart);
    memcpy(fake->uart, data, length);
    fake->uart_length = length;
}

static void queue_packet(void *opaque, const uint8_t *data, size_t length)
{
    FakeHardware *fake = (FakeHardware *)opaque;
    assert(length <= sizeof fake->packet);
    memcpy(fake->packet, data, length);
    fake->packet_length = length;
}

static uint16_t read_adc(void *opaque) { (void)opaque; return 0x200u; }
static uint8_t read_dac(void *opaque) { (void)opaque; return 0x05u; }
static bool tx_active(void *opaque) { return ((FakeHardware *)opaque)->tx_active; }
static uint8_t next_tx_wave(void *opaque) { (void)opaque; return 0x20u; }
static void demod(void *opaque, int16_t sample)
{
    FakeHardware *fake = (FakeHardware *)opaque;
    assert(fake->dac_calls == 0); /* Decoder always sees the sample first. */
    fake->sample = sample;
    fake->demod_calls++;
}
static void write_dac(void *opaque, uint8_t value)
{
    FakeHardware *fake = (FakeHardware *)opaque;
    fake->dac = value;
    fake->dac_calls++;
}

static void test_wire_vectors(void)
{
    static const uint8_t payload[] = { 4, 0xC0, 0xDB, 0 };
    static const uint8_t expected[] = { 0xC0, 0x11, 4, 0xDB, 0xDC,
                                        0xDB, 0xDD, 0, 0xC0 };
    uint8_t encoded[32];
    H9WireDecoder decoder;
    size_t i;
    assert(h9_wire_encode(0x11u, payload, sizeof payload,
                          encoded, sizeof encoded) == sizeof expected);
    assert(memcmp(encoded, expected, sizeof expected) == 0);
    h9_wire_reset(&decoder);
    for (i = 0; i < sizeof expected - 1; ++i)
        assert(h9_wire_feed(&decoder, encoded[i]) == H9_WIRE_MORE);
    assert(h9_wire_feed(&decoder, encoded[i]) == H9_WIRE_FRAME);
    assert(decoder.command == 0x11u);
    assert(decoder.length == sizeof payload);
    assert(memcmp(decoder.payload, payload, sizeof payload) == 0);
    h9_wire_reset(&decoder);
    assert(h9_wire_feed(&decoder, 0xC0u) == H9_WIRE_MORE);
    assert(h9_wire_feed(&decoder, 0x10u) == H9_WIRE_MORE);
    assert(h9_wire_feed(&decoder, 0xDBu) == H9_WIRE_MORE);
    assert(h9_wire_feed(&decoder, 0x01u) == H9_WIRE_REJECTED);
}

static void test_main_policy(void)
{
    H9MainAprsConfig cfg = { true, true, H9_RX_B, H9_TX_B_LOCK };
    H9AudioPlan audio;
    H9TxPlan tx;
    audio = h9_main_audio_plan(&cfg, H9_SIDE_B);
    assert(audio.decoder_feed && !audio.speaker_audio && !audio.bluetooth_audio);
    audio = h9_main_audio_plan(&cfg, H9_SIDE_A);
    assert(!audio.decoder_feed && audio.speaker_audio && audio.bluetooth_audio);
    tx = h9_main_tx_plan(&cfg, H9_SIDE_A, 445390000u, 144390000u);
    assert(!tx.allowed); /* Lock never silently follows A. */
    tx = h9_main_tx_plan(&cfg, H9_SIDE_B, 445390000u, 144390000u);
    assert(tx.allowed && tx.side == H9_SIDE_B && tx.expected_tx_hz == 144390000u);
    assert(!h9_main_tx_prepared(&tx, H9_SIDE_A, 445390000u));
    assert(!h9_main_tx_prepared(&tx, H9_SIDE_B, 445390000u));
    assert(h9_main_tx_prepared(&tx, H9_SIDE_B, 144390000u));
    cfg.tx_choice = H9_TX_B_AUTO;
    tx = h9_main_tx_plan(&cfg, H9_SIDE_A, 445390000u, 144390000u);
    assert(tx.allowed && tx.side == H9_SIDE_B);
}

typedef struct { uint8_t bytes[32]; size_t length; } BitOutput;
static bool emit_bitstream_byte(void *opaque, uint8_t byte)
{
    BitOutput *output = (BitOutput *)opaque;
    if (output->length == sizeof output->bytes) return false;
    output->bytes[output->length++] = byte;
    return true;
}

static void feed_lsb_byte(H9AprsBitstream *state, BitOutput *output, uint8_t byte)
{
    unsigned bit;
    for (bit = 0; bit < 8; ++bit)
        assert(h9_aprs_bitstream_feed(state, (uint8_t)((byte >> bit) & 1u),
                                       emit_bitstream_byte, output));
}

static void test_recovered_bit_path(void)
{
    H9AprsBitstream state;
    BitOutput output = { { 0 }, 0 };
    h9_aprs_bitstream_reset(&state);
    feed_lsb_byte(&state, &output, 0x7Eu);
    feed_lsb_byte(&state, &output, 0x55u);
    feed_lsb_byte(&state, &output, 0x7Eu);
    assert(output.length == 3);
    assert(output.bytes[0] == 0x7Eu && output.bytes[1] == 0x55u &&
           output.bytes[2] == 0x7Eu);

    h9_aprs_bitstream_reset(&state);
    output.length = 0;
    feed_lsb_byte(&state, &output, 0x7Eu);
    feed_lsb_byte(&state, &output, 0xF8u); /* Ends in five 1 bits. */
    assert(h9_aprs_bitstream_feed(&state, 0, emit_bitstream_byte, &output));
    feed_lsb_byte(&state, &output, 0x7Eu);
    assert(output.length == 3 && output.bytes[1] == 0xF8u);

    h9_aprs_bitstream_reset(&state);
    output.length = 0;
    feed_lsb_byte(&state, &output, 0x7Eu);
    feed_lsb_byte(&state, &output, 0x1Bu);
    feed_lsb_byte(&state, &output, 0x7Eu);
    assert(output.length == 4 && output.bytes[1] == 0x1Bu &&
           output.bytes[2] == 0x1Bu);
}

static void test_aprs_byte_queue(void)
{
    uint8_t storage[4] = { 0 };
    uint8_t byte = 0;
    H9AprsRing ring;
    assert(h9_aprs_ring_init(&ring, storage, sizeof storage));
    assert(h9_aprs_ring_empty(&ring));
    assert(h9_aprs_ring_push(&ring, 0x7Eu));
    assert(h9_aprs_ring_push(&ring, 0x55u));
    assert(h9_aprs_ring_push(&ring, 0x7Eu));
    assert(h9_aprs_ring_full(&ring));
    assert(!h9_aprs_ring_push(&ring, 0x99u));
    assert(h9_aprs_ring_pop(&ring, &byte) && byte == 0x7Eu);
    assert(h9_aprs_ring_push(&ring, 0x99u)); /* tail wraps */
    assert(h9_aprs_ring_pop(&ring, &byte) && byte == 0x55u);
    assert(h9_aprs_ring_pop(&ring, &byte) && byte == 0x7Eu);
    assert(h9_aprs_ring_pop(&ring, &byte) && byte == 0x99u);
    assert(h9_aprs_ring_empty(&ring));
}

static void feed_frame(H9AprsController *controller, uint8_t command,
                       const uint8_t *payload, size_t length)
{
    uint8_t encoded[H9_APRS_ENCODED_CAPACITY];
    size_t size = h9_wire_encode(command, payload, length, encoded,
                                 sizeof encoded);
    size_t i;
    assert(size != 0);
    for (i = 0; i < size; ++i)
        h9_aprs_controller_uart_byte(controller, encoded[i]);
}

static void test_aprs_controller(void)
{
    FakeHardware fake = { 0 };
    H9AprsHardware hw = { 0 };
    H9AprsController controller;
    uint8_t send_packet[] = { 4, 0x41, 0xC0, 0xDB };
    uint8_t query = 1;
    hw.context = &fake;
    hw.uart_write = uart_write;
    hw.queue_packet_tx = queue_packet;
    hw.read_adc = read_adc;
    hw.demod_sample = demod;
    hw.read_dac_port = read_dac;
    hw.write_dac_port = write_dac;
    hw.tx_wave_active = tx_active;
    hw.next_tx_wave_sample = next_tx_wave;
    h9_aprs_controller_init(&controller, &hw);
    feed_frame(&controller, 0x11u, send_packet, sizeof send_packet);
    assert(fake.packet_length == 3);
    assert(memcmp(fake.packet, send_packet + 1, 3) == 0);
    feed_frame(&controller, 0x20u, &query, 1);
    assert(fake.uart_length == 12);
    assert(fake.uart[0] == 0xC0u && fake.uart[1] == 0x21u);
    assert(fake.uart[2] == 1 && memcmp(fake.uart + 3, "V1.0.15", 7) == 0);
    h9_aprs_controller_adc_tick(&controller);
    assert(fake.demod_calls == 1 && fake.sample == 0);
    assert(fake.dac_calls == 1 && fake.dac == 0x85u);
    fake.tx_active = true;
    fake.dac_calls = 0;
    h9_aprs_controller_adc_tick(&controller);
    assert(fake.demod_calls == 2 && fake.dac == 0x45u);
}

int main(void)
{
    test_wire_vectors();
    test_main_policy();
    test_recovered_bit_path();
    test_aprs_byte_queue();
    test_aprs_controller();
    return 0;
}
