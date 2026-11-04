#include "h9_aprs_controller.h"
#include "h9_aprs_demod.h"
#include "h9_aprs_frame.h"
#include "h9_aprs_bitstream.h"
#include "h9_aprs_ring.h"
#include "h9_aprs_tx.h"
#include "h9_aprs_tx_gate.h"
#include "h9_aprs_tx_packet.h"
#include "h9_aprs_tx_service.h"
#include "h9_aprs_oscillator.h"
#include "h9_main_aprs.h"
#include "h9_main_aprs_metadata.h"
#include "h9_main_aprs_receive.h"
#include "h9_main_aprs_wire.h"
#include "h9_main_ax25.h"
#include "h9_main_beacon.h"
#include <assert.h>
#include <string.h>

typedef struct {
    uint8_t uart[H9_APRS_ENCODED_CAPACITY];
    size_t uart_length;
    uint8_t packet[H9_APRS_MAX_RECEIVE];
    size_t packet_length;
    uint8_t afsk_packet[H9_APRS_MAX_RECEIVE];
    size_t afsk_packet_length;
    H9AprsTxService *tx_service;
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

static void transmit_afsk_packet(void *opaque, const uint8_t *data,
                                 size_t length)
{
    FakeHardware *fake = (FakeHardware *)opaque;
    assert(length <= sizeof fake->afsk_packet);
    memcpy(fake->afsk_packet, data, length);
    fake->afsk_packet_length = length;
}

static void submit_service_packet(void *opaque, const uint8_t *data,
                                  size_t length)
{
    FakeHardware *fake = (FakeHardware *)opaque;
    assert(fake->tx_service != NULL);
    assert(h9_aprs_tx_service_submit(fake->tx_service, data, length));
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

static void test_main_aprs_wire_envelope(void)
{
    static const uint8_t payload[] = { 0x41u, 0xC0u, 0xDBu };
    static const uint8_t expected[] = {
        0xC0u, 0xC0u, 0xC0u, 0xC0u, 0xC0u, 0x00u,
        0x41u, 0xDBu, 0xDCu, 0xDBu, 0xDDu,
        0xC0u, 0xC0u, 0xC0u
    };
    uint8_t output[H9_MAIN_APRS_WIRE_CAPACITY];
    uint8_t max_payload[H9_MAIN_APRS_MAX_PAYLOAD];
    H9WireDecoder decoder;
    size_t i;
    unsigned frames = 0;
    assert(h9_main_aprs_wire_encode(0, payload, sizeof payload, output,
                                     15) == 0); /* stock's conservative bound */
    assert(h9_main_aprs_wire_encode(0, NULL, 0, output,
                                     sizeof output) == 0);
    assert(h9_main_aprs_wire_encode(0, payload, sizeof payload, output,
                                     sizeof output) == sizeof expected);
    assert(memcmp(output, expected, sizeof expected) == 0);
    h9_wire_reset(&decoder);
    for (i = 0; i < sizeof expected; ++i) {
        if (h9_wire_feed(&decoder, output[i]) == H9_WIRE_FRAME) {
            frames++;
            assert(decoder.command == 0 && decoder.length == sizeof payload);
            assert(memcmp(decoder.payload, payload, sizeof payload) == 0);
            h9_wire_reset(&decoder);
        }
    }
    assert(frames == 1);
    memset(max_payload, 0xC0, sizeof max_payload);
    assert(h9_main_aprs_wire_encode(0, max_payload, sizeof max_payload,
                                     output, sizeof output) == 2047u);
    assert(output[2046] == 0xC0u);
}

static void test_main_aprs_wire_receiver(void)
{
    static const uint8_t frame[] = {
        0xC0u, 0xC0u, 0x00u, 0x41u, 0xDBu, 0xDCu,
        0xDBu, 0xDDu, 0xC0u
    };
    static const uint8_t payload[] = { 0x41u, 0xC0u, 0xDBu };
    H9MainWireReceiver receiver;
    size_t i;
    h9_main_wire_reset(&receiver);
    assert(h9_main_wire_feed(&receiver, 0x44u) == H9_MAIN_WIRE_MORE);
    for (i = 0; i < sizeof frame - 1; ++i)
        assert(h9_main_wire_feed(&receiver, frame[i]) == H9_MAIN_WIRE_MORE);
    assert(h9_main_wire_feed(&receiver, frame[i]) == H9_MAIN_WIRE_FRAME);
    assert(receiver.command == 0 && receiver.length == sizeof payload);
    assert(memcmp(receiver.payload, payload, sizeof payload) == 0);

    assert(h9_main_wire_feed(&receiver, 0xC0u) == H9_MAIN_WIRE_MORE);
    assert(h9_main_wire_feed(&receiver, 0x10u) == H9_MAIN_WIRE_MORE);
    assert(h9_main_wire_feed(&receiver, 0xDBu) == H9_MAIN_WIRE_MORE);
    assert(h9_main_wire_feed(&receiver, 0x01u) == H9_MAIN_WIRE_MORE);
    assert(h9_main_wire_feed(&receiver, 0xC0u) == H9_MAIN_WIRE_FRAME);
    assert(receiver.command == 0x10u && receiver.length == 1u);
    assert(receiver.payload[0] == 0x01u);

    assert(h9_main_wire_feed(&receiver, 0xC0u) == H9_MAIN_WIRE_MORE);
    assert(h9_main_wire_feed(&receiver, 0x21u) == H9_MAIN_WIRE_MORE);
    for (i = 0; i < H9_MAIN_APRS_RECEIVE_CAPACITY; ++i)
        assert(h9_main_wire_feed(&receiver, 0x42u) == H9_MAIN_WIRE_MORE);
    assert(receiver.length == H9_MAIN_APRS_RECEIVE_CAPACITY);
    assert(h9_main_wire_feed(&receiver, 0x43u) == H9_MAIN_WIRE_REJECTED);
    assert(receiver.state == 0 && receiver.length == 0);
    assert(h9_main_wire_feed(&receiver, 0xC0u) == H9_MAIN_WIRE_MORE);
    assert(h9_main_wire_feed(&receiver, 0x21u) == H9_MAIN_WIRE_MORE);
    assert(h9_main_wire_feed(&receiver, 0xC0u) == H9_MAIN_WIRE_FRAME);
    assert(receiver.command == 0x21u && receiver.length == 0u);
}

static void test_main_ax25_packet(void)
{
    static const uint8_t info[] = { '!', 'T', 'E', 'S', 'T' };
    static const uint8_t expected[] = {
        0x82u, 0xA0u, 0xA4u, 0xA6u, 0x40u, 0x40u, 0x60u,
        0x96u, 0x88u, 0x72u, 0x86u, 0xA4u, 0xA2u, 0x61u,
        0x03u, 0xF0u, '!', 'T', 'E', 'S', 'T'
    };
    H9MainAx25Packet packet = { { 0 }, { 0 }, { { 0 } }, 0, 0, info,
                                sizeof info };
    H9MainAx25Decoded decoded;
    uint8_t output[H9_MAIN_AX25_MAX_PACKET];
    uint8_t max_info[496] = { 0 };
    memcpy(packet.destination, "APRS", 4);
    memcpy(packet.source, "KD9CRQ", 6);
    assert(h9_main_ax25_encode(&packet, output, sizeof output) ==
           sizeof expected);
    assert(memcmp(output, expected, sizeof expected) == 0);
    assert(h9_main_ax25_decode(output, sizeof expected, &decoded));
    assert(decoded.digipeater_count == 0 && decoded.control == 0x03u);
    assert(memcmp(decoded.destination, "APRS", 4) == 0);
    assert(decoded.destination[4] == 0 && decoded.destination[5] == 0);
    assert(memcmp(decoded.source, "KD9CRQ", 6) == 0);
    assert(decoded.pid == 0xF0u && decoded.information_length == sizeof info);
    assert(memcmp(decoded.information, info, sizeof info) == 0);
    assert(h9_main_ax25_is_aprs_payload(&decoded));
    packet.digipeater_count = 1;
    memcpy(packet.digipeaters[0], "WIDE1", 5);
    packet.digipeaters[0][6] = 0x81u;
    assert(h9_main_ax25_encode(&packet, output, sizeof output) ==
           sizeof expected + 7u);
    assert(output[13] == 0x60u && output[20] == 0xE3u);
    assert(h9_main_ax25_decode(output, sizeof expected + 7u, &decoded));
    assert(decoded.digipeater_count == 1u);
    assert(memcmp(decoded.digipeaters[0], "WIDE1", 5) == 0);
    assert(decoded.digipeaters[0][6] == 0x81u);
    packet.digipeater_count = H9_MAIN_AX25_MAX_DIGI;
    assert(h9_main_ax25_encode(&packet, output, sizeof output) ==
           sizeof info + 16u + 7u * H9_MAIN_AX25_MAX_DIGI);
    assert(h9_main_ax25_decode(output,
                                sizeof info + 16u + 7u * H9_MAIN_AX25_MAX_DIGI,
                                &decoded));
    assert(decoded.digipeater_count == H9_MAIN_AX25_MAX_DIGI);
    output[55] &= 0xFEu; /* Sixth address no longer ends the path. */
    assert(!h9_main_ax25_decode(output,
                                 sizeof info + 16u + 7u * H9_MAIN_AX25_MAX_DIGI,
                                 &decoded));
    packet.digipeater_count = 0;
    packet.information = max_info;
    packet.information_length = sizeof max_info;
    assert(h9_main_ax25_encode(&packet, output, sizeof output) ==
           sizeof output);
    assert(h9_main_ax25_decode(output, sizeof output, &decoded));
    assert(decoded.information_length == H9_MAIN_AX25_MAX_INFORMATION);
    packet.information_length++;
    assert(h9_main_ax25_encode(&packet, output, sizeof output) == 0);
    assert(!h9_main_ax25_decode(expected, 15u, &decoded));
    output[13] = 0x60u;
    assert(!h9_main_ax25_decode(output, 16u, &decoded));
    decoded.pid = 0xF1u;
    assert(!h9_main_ax25_is_aprs_payload(&decoded));
    decoded.pid = 0x30u;
    assert(h9_main_ax25_is_aprs_payload(&decoded));
}

static void test_main_received_aprs_packet(void)
{
    static const uint8_t info[] = { '!', 'H', 'I' };
    H9MainAx25Packet packet = { 0 };
    H9MainAx25Decoded decoded;
    H9MainWireReceiver receiver;
    uint8_t raw[H9_MAIN_AX25_MAX_PACKET];
    uint8_t serial[H9_APRS_ENCODED_CAPACITY];
    size_t raw_length;
    size_t serial_length;
    size_t i;
    memcpy(packet.destination, "APRS", 4);
    memcpy(packet.source, "KD9CRQ", 6);
    packet.information = info;
    packet.information_length = sizeof info;
    raw_length = h9_main_ax25_encode(&packet, raw, sizeof raw);
    assert(raw_length == 19u);
    serial_length = h9_wire_encode(0x00u, raw, raw_length,
                                   serial, sizeof serial);
    assert(serial_length > raw_length);
    h9_main_wire_reset(&receiver);
    for (i = 0; i < serial_length - 1; ++i)
        assert(h9_main_wire_feed(&receiver, serial[i]) == H9_MAIN_WIRE_MORE);
    assert(h9_main_wire_feed(&receiver, serial[i]) == H9_MAIN_WIRE_FRAME);
    assert(h9_main_aprs_packet_from_frame(&receiver, &decoded));
    assert(memcmp(decoded.source, "KD9CRQ", 6) == 0);
    assert(decoded.information_length == sizeof info);
    assert(memcmp(decoded.information, info, sizeof info) == 0);
    assert(h9_main_wire_feed(&receiver, 0x42u) == H9_MAIN_WIRE_MORE);
    assert(!h9_main_aprs_packet_from_frame(&receiver, &decoded));

    serial_length = h9_wire_encode(0x10u, raw, raw_length,
                                   serial, sizeof serial);
    h9_main_wire_reset(&receiver);
    for (i = 0; i < serial_length; ++i)
        (void)h9_main_wire_feed(&receiver, serial[i]);
    assert(receiver.frame_ready);
    assert(!h9_main_aprs_packet_from_frame(&receiver, &decoded));
}

typedef struct {
    uint8_t sentence[256];
    size_t length;
    unsigned count;
} GpsOutput;

static void capture_gps_sentence(void *context, const uint8_t *sentence,
                                 size_t length)
{
    GpsOutput *output = (GpsOutput *)context;
    assert(length <= sizeof output->sentence);
    memcpy(output->sentence, sentence, length);
    output->length = length;
    output->count++;
}

static void test_main_gps_sentence_frames(void)
{
    static const uint8_t first[] = "noise$BAD$GPR";
    static const uint8_t second[] = "MC,1*00\r\n";
    static const uint8_t expected[] = "$GPRMC,1*00\r\n";
    H9MainGpsCollector collector;
    H9MainWireReceiver receiver;
    GpsOutput output = { { 0 }, 0, 0 };
    uint8_t serial[H9_APRS_ENCODED_CAPACITY];
    size_t serial_length;
    size_t i;
    h9_main_gps_init(&collector);
    h9_main_wire_reset(&receiver);
    serial_length = h9_wire_encode(0x10u, first, sizeof first - 1u,
                                   serial, sizeof serial);
    for (i = 0; i < serial_length; ++i)
        (void)h9_main_wire_feed(&receiver, serial[i]);
    assert(receiver.frame_ready);
    assert(h9_main_aprs_gps_from_frame(&collector, &receiver,
                                        capture_gps_sentence, &output) == 0);
    serial_length = h9_wire_encode(0x10u, second, sizeof second - 1u,
                                   serial, sizeof serial);
    for (i = 0; i < serial_length; ++i)
        (void)h9_main_wire_feed(&receiver, serial[i]);
    assert(receiver.frame_ready);
    assert(h9_main_aprs_gps_from_frame(&collector, &receiver,
                                        capture_gps_sentence, &output) == 1);
    assert(output.count == 1 && output.length == sizeof expected - 1u);
    assert(memcmp(output.sentence, expected, output.length) == 0);
    assert(collector.length == 0);
}

static void test_main_gpio_query_reply(void)
{
    static const uint8_t query[] = { 0xC0u, 0x11u, 3u, 0xC0u };
    H9MainWireReceiver main_receiver;
    H9WireDecoder module_receiver;
    uint8_t reply[H9_MAIN_APRS_WIRE_CAPACITY];
    size_t reply_length;
    size_t i;
    unsigned frames = 0;
    h9_main_wire_reset(&main_receiver);
    for (i = 0; i < sizeof query - 1u; ++i)
        assert(h9_main_wire_feed(&main_receiver, query[i]) ==
               H9_MAIN_WIRE_MORE);
    assert(h9_main_wire_feed(&main_receiver, query[i]) == H9_MAIN_WIRE_FRAME);
    reply_length = h9_main_aprs_gpio_reply_from_frame(&main_receiver, reply,
                                                        sizeof reply);
    assert(reply_length != 0);
    h9_wire_reset(&module_receiver);
    for (i = 0; i < reply_length; ++i) {
        if (h9_wire_feed(&module_receiver, reply[i]) == H9_WIRE_FRAME) {
            frames++;
            assert(module_receiver.command == 0x11u);
            assert(module_receiver.length == 2u);
            assert(module_receiver.payload[0] == 3u);
            assert(module_receiver.payload[1] == 0u);
            h9_wire_reset(&module_receiver);
        }
    }
    assert(frames == 1);
    main_receiver.payload[0] = 2u;
    assert(h9_main_aprs_gpio_reply_from_frame(&main_receiver, reply,
                                               sizeof reply) == 0);
}

static void test_main_policy(void)
{
    H9MainAprsConfig cfg = { true, true, H9_RX_B, H9_TX_B_LOCK };
    H9AudioPlan audio;
    H9TxPlan tx;
    uint8_t side_state = 0x2Du;
    assert(h9_main_side_from_state(side_state) == H9_SIDE_A);
    side_state = h9_main_track_side_bit(side_state, true, 144390000u,
                                         445390000u, 144390000u);
    assert(side_state == 0xADu);
    assert(h9_main_side_from_state(side_state) == H9_SIDE_B);
    assert(h9_main_track_side_bit(side_state, false, 445390000u,
                                  445390000u, 144390000u) == side_state);
    assert(h9_main_track_side_bit(side_state, true, 145000000u,
                                  445390000u, 144390000u) == side_state);
    assert(h9_main_track_side_bit(side_state, true, 445390000u,
                                  445390000u, 144390000u) == 0x2Du);
    /* The stock branch tests A first when both channel values match. */
    assert(h9_main_track_side_bit(side_state, true, 144390000u,
                                  144390000u, 144390000u) == 0x2Du);
    audio = h9_main_audio_plan(&cfg, H9_SIDE_B);
    assert(audio.decoder_feed && !audio.speaker_audio && !audio.bluetooth_audio);
    audio = h9_main_audio_plan(&cfg, H9_SIDE_A);
    assert(!audio.decoder_feed && audio.speaker_audio && audio.bluetooth_audio);
    tx = h9_main_tx_plan(&cfg, H9_SIDE_A, 445390000u, 144390000u);
    assert(!tx.allowed); /* Lock never silently follows A. */
    tx = h9_main_tx_plan(&cfg, H9_SIDE_B, 445390000u, 144390000u);
    assert(tx.allowed && tx.side == H9_SIDE_B && tx.expected_tx_hz == 144390000u);
    assert(!tx.restore_side_after_send);
    tx = h9_main_tx_plan(&cfg, h9_main_side_from_state(side_state),
                         445390000u, 144390000u);
    assert(tx.allowed && tx.side == H9_SIDE_B);
    assert(!tx.restore_side_after_send);
    assert(!h9_main_tx_prepared(&tx, H9_SIDE_A, 445390000u));
    assert(!h9_main_tx_prepared(&tx, H9_SIDE_B, 445390000u));
    assert(h9_main_tx_prepared(&tx, H9_SIDE_B, 144390000u));
    cfg.tx_choice = H9_TX_B_AUTO;
    tx = h9_main_tx_plan(&cfg, H9_SIDE_A, 445390000u, 144390000u);
    assert(tx.allowed && tx.side == H9_SIDE_B && tx.restore_side_after_send);
    tx = h9_main_tx_plan(&cfg, H9_SIDE_B, 445390000u, 144390000u);
    assert(tx.allowed && !tx.restore_side_after_send);
    assert(h9_main_rx_channel_matches(H9_RX_B, 144390000u,
                                      445390000u, 144390000u));
    assert(!h9_main_rx_channel_matches(H9_RX_B, 445390000u,
                                       445390000u, 144390000u));
    assert(!h9_main_rx_channel_matches(H9_RX_B, 145000000u,
                                       445390000u, 144390000u));
    assert(h9_main_rx_channel_matches(H9_RX_OFF, 145000000u,
                                      445390000u, 144390000u));
}

static void test_timed_beacon_scheduler(void)
{
    H9BeaconScheduler scheduler = { 5999u };
    H9BeaconDecision decision;
    decision = h9_main_beacon_tick(&scheduler, 1, 60, 0, 0);
    assert(decision.call_tx_selector);
    assert(decision.restore_receive_if_selector_succeeds);
    assert(scheduler.counter == 0);
    scheduler.counter = 5999u;
    decision = h9_main_beacon_tick(&scheduler, 1, 60, 1, 0);
    assert(!decision.call_tx_selector && scheduler.counter == 0);
    scheduler.counter = 5999u;
    decision = h9_main_beacon_tick(&scheduler, 1, 60, 1, 1);
    assert(decision.call_tx_selector);
    assert(!decision.restore_receive_if_selector_succeeds);
    scheduler.counter = 47u;
    decision = h9_main_beacon_tick(&scheduler, 1, 0, 0, 0);
    assert(!decision.call_tx_selector && scheduler.counter == 47u);
    decision = h9_main_beacon_tick(&scheduler, 0, 60, 0, 0);
    assert(!decision.call_tx_selector && scheduler.counter == 0);
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

static void test_stock_waveform_vectors(void)
{
    static const uint8_t step_40[] = { 218, 255, 217, 127, 37, 0, 38, 128 };
    static const uint8_t step_75[] = { 254, 160, 10, 63, 227, 219, 53, 16 };
    H9AprsOscillator oscillator;
    size_t index;
    h9_aprs_oscillator_init(&oscillator);
    for (index = 0; index < sizeof step_40; ++index)
        assert(h9_aprs_oscillator_next(&oscillator) == step_40[index]);
    h9_aprs_oscillator_init(&oscillator);
    assert(h9_aprs_oscillator_set_step(&oscillator, 0x75u));
    for (index = 0; index < sizeof step_75; ++index)
        assert(h9_aprs_oscillator_next(&oscillator) == step_75[index]);
    assert(!h9_aprs_oscillator_set_step(&oscillator, 0));
}

static void test_stock_demod_delay(void)
{
    H9AprsDemod demod_state;
    unsigned count;
    uint8_t byte;
    h9_aprs_demod_init(&demod_state);
    for (count = 0; count < 4; ++count) {
        assert(h9_aprs_demod_sample(&demod_state, 100));
        assert(demod_state.correlation_current == 0);
    }
    assert(h9_aprs_demod_sample(&demod_state, 100));
    assert(demod_state.correlation_current == 2500);
    assert(!h9_aprs_demod_pop_byte(&demod_state, &byte));
    assert(!h9_aprs_demod_sample(&demod_state, 128));
}

static void test_stock_tx_scheduler(void)
{
    H9AprsTx tx;
    uint8_t sample = 0;
    unsigned count;
    h9_aprs_tx_init(&tx, 0, 0);
    assert(h9_aprs_tx_enqueue(&tx, 0xFFu));
    for (count = 0; count < 40; ++count) {
        assert(h9_aprs_tx_next_sample(&tx, &sample));
        if (count == 0) assert(sample == 218u);
    }
    assert(tx.oscillator.step == 0x40u && tx.bit_mask == 0x20u);
    assert(h9_aprs_tx_next_sample(&tx, &sample));
    assert(tx.oscillator.step == 0x75u && tx.bit_mask == 0x20u);
    for (count = 41; count < 72; ++count)
        assert(h9_aprs_tx_next_sample(&tx, &sample));
    assert(!h9_aprs_tx_next_sample(&tx, &sample));

    h9_aprs_tx_init(&tx, 7, 7); /* one leading and one trailing flag */
    assert(h9_aprs_tx_enqueue(&tx, 0));
    for (count = 0; count < 192; ++count)
        assert(h9_aprs_tx_next_sample(&tx, &sample));
    assert(!h9_aprs_tx_next_sample(&tx, &sample));

    h9_aprs_tx_init(&tx, 0, 0);
    assert(h9_aprs_tx_enqueue(&tx, 0x1Bu));
    assert(!h9_aprs_tx_next_sample(&tx, &sample));
}

typedef struct {
    uint8_t bytes[H9_APRS_MAX_RECEIVE];
    size_t length;
    unsigned packets;
} PacketOutput;

static void emit_packet(void *opaque, const uint8_t *data, size_t length)
{
    PacketOutput *output = (PacketOutput *)opaque;
    assert(length <= sizeof output->bytes);
    memcpy(output->bytes, data, length);
    output->length = length;
    output->packets++;
}

static void test_aprs_frame_fcs(void)
{
    static const uint8_t check[] = "123456789";
    uint16_t crc = 0xFFFFu;
    H9AprsFrame frame;
    PacketOutput output = { { 0 }, 0, 0 };
    unsigned i;
    for (i = 0; i < 9; ++i)
        crc = h9_aprs_frame_crc_byte(crc, check[i]);
    assert(crc == 0x6F91u); /* CRC-16/X-25 check value is ~0x6F91. */

    h9_aprs_frame_init(&frame);
    assert(!h9_aprs_frame_feed(&frame, 0x7Eu, emit_packet, &output));
    for (i = 0; i < 18; ++i)
        assert(!h9_aprs_frame_feed(&frame, (uint8_t)i,
                                   emit_packet, &output));
    assert(!h9_aprs_frame_feed(&frame, 0x45u, emit_packet, &output));
    assert(!h9_aprs_frame_feed(&frame, 0x87u, emit_packet, &output));
    assert(h9_aprs_frame_feed(&frame, 0x7Eu, emit_packet, &output));
    assert(output.packets == 1 && output.length == 18);
    for (i = 0; i < 18; ++i) assert(output.bytes[i] == (uint8_t)i);

    for (i = 0; i < 18; ++i)
        (void)h9_aprs_frame_feed(&frame, (uint8_t)i, emit_packet, &output);
    (void)h9_aprs_frame_feed(&frame, 0x45u, emit_packet, &output);
    (void)h9_aprs_frame_feed(&frame, 0x86u, emit_packet, &output);
    assert(!h9_aprs_frame_feed(&frame, 0x7Eu, emit_packet, &output));
    assert(output.packets == 1);
}

static void test_aprs_tx_packet_encoder(void)
{
    static const uint8_t reserved[] = { 0x7Eu, 0x7Fu, 0x1Bu };
    static const uint8_t expected[] = {
        0x7Eu, 0x1Bu, 0x7Eu, 0x1Bu, 0x7Fu,
        0x1Bu, 0x1Bu, 0x51u, 0x8Bu, 0x7Eu
    };
    H9AprsTxPacket encoder;
    H9AprsTx tx;
    H9AprsFrame frame;
    PacketOutput output = { { 0 }, 0, 0 };
    uint8_t packet[80];
    uint8_t byte;
    unsigned i;
    assert(h9_aprs_tx_packet_init(&encoder, reserved, sizeof reserved));
    for (i = 0; i < sizeof expected; ++i) {
        assert(h9_aprs_tx_packet_next(&encoder, &byte));
        assert(byte == expected[i]);
    }
    assert(h9_aprs_tx_packet_done(&encoder));
    assert(encoder.crc == 0xF0B8u);
    assert(!h9_aprs_tx_packet_next(&encoder, &byte));

    h9_aprs_tx_init(&tx, 0, 0);
    for (i = 0; i < 62; ++i) assert(h9_aprs_tx_enqueue(&tx, 0x55u));
    assert(h9_aprs_tx_packet_init(&encoder, reserved, sizeof reserved));
    assert(h9_aprs_tx_packet_pump(&encoder, &tx) == 1); /* opening flag */
    assert(h9_aprs_ring_pop(&tx.queue, &byte));
    assert(h9_aprs_tx_packet_pump(&encoder, &tx) == 0);
    assert(encoder.index == 0 && encoder.pending_escape == 0);
    assert(h9_aprs_ring_pop(&tx.queue, &byte));
    assert(h9_aprs_tx_packet_pump(&encoder, &tx) == 2);
    assert(encoder.index == 1 && encoder.pending_escape == 0);

    for (i = 0; i < sizeof packet; ++i) packet[i] = (uint8_t)(0x30u + i);
    h9_aprs_tx_init(&tx, 0, 0);
    h9_aprs_frame_init(&frame);
    assert(h9_aprs_tx_packet_init(&encoder, packet, sizeof packet));
    for (i = 0; i < 10 && !h9_aprs_tx_packet_done(&encoder); ++i) {
        assert(h9_aprs_tx_packet_pump(&encoder, &tx) != 0);
        while (h9_aprs_ring_pop(&tx.queue, &byte))
            (void)h9_aprs_frame_feed(&frame, byte, emit_packet, &output);
    }
    assert(h9_aprs_tx_packet_done(&encoder));
    assert(output.packets == 1 && output.length == sizeof packet);
    assert(memcmp(output.bytes, packet, sizeof packet) == 0);
}

static void test_aprs_tx_gate(void)
{
    H9AprsTxGate gate;
    h9_aprs_tx_gate_init(&gate, 0);
    assert(h9_aprs_tx_gate_step(&gate, true, false, false, 255) ==
           H9_APRS_TX_WAIT);
    assert(gate.random_state == 0); /* busy channel consumes no draw */
    assert(h9_aprs_tx_gate_step(&gate, true, true, true, 255) ==
           H9_APRS_TX_DROP);
    assert(gate.random_state == 0);
    assert(h9_aprs_tx_gate_step(&gate, false, false, false, 28) ==
           H9_APRS_TX_WAIT);
    assert(gate.random_state == 0x3039u); /* first LCG state, draw 28 */
    h9_aprs_tx_gate_init(&gate, 0);
    assert(h9_aprs_tx_gate_step(&gate, false, false, false, 29) ==
           H9_APRS_TX_SEND);
    h9_aprs_tx_gate_init(&gate, 0);
    assert(h9_aprs_tx_gate_step(&gate, false, false, false, 0) ==
           H9_APRS_TX_WAIT);
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

static void test_aprs_tx_service(void)
{
    H9AprsTxService service;
    H9AprsController controller;
    H9AprsDemod demodulator;
    H9AprsFrame receive_frame;
    PacketOutput decoded = { { 0 }, 0, 0 };
    H9AprsHardware hw = { 0 };
    FakeHardware fake = { 0 };
    H9MainAx25Packet ax25 = { 0 };
    uint8_t information[80];
    uint8_t packet[H9_MAIN_AX25_MAX_PACKET];
    uint8_t expected[H9_MAIN_AX25_MAX_PACKET];
    uint8_t serial[H9_MAIN_APRS_WIRE_CAPACITY];
    uint8_t sample;
    uint8_t receive_byte;
    size_t samples = 0;
    size_t serial_length;
    size_t packet_length;
    unsigned i;
    for (i = 0; i < sizeof information; ++i)
        information[i] = (uint8_t)(0x30u + i);
    information[0] = 0x1Bu;
    information[1] = 0xC0u;
    information[2] = 0xDBu;
    memcpy(ax25.destination, "APRS", 4);
    memcpy(ax25.source, "KD9CRQ", 6);
    ax25.information = information;
    ax25.information_length = sizeof information;
    packet_length = h9_main_ax25_encode(&ax25, packet, sizeof packet);
    assert(packet_length == sizeof information + 16u);
    memcpy(expected, packet, packet_length);
    h9_aprs_tx_service_init(&service, 0, 300, 100);
    h9_aprs_demod_init(&demodulator);
    h9_aprs_frame_init(&receive_frame);
    fake.tx_service = &service;
    hw.context = &fake;
    hw.transmit_aprs_packet = submit_service_packet;
    h9_aprs_controller_init(&controller, &hw);
    serial_length = h9_main_aprs_wire_encode(0x00u, packet, packet_length,
                                              serial, sizeof serial);
    assert(serial_length != 0);
    for (i = 0; i < serial_length; ++i)
        h9_aprs_controller_uart_byte(&controller, serial[i]);
    assert(service.phase == H9_APRS_TX_WAITING);
    assert(memcmp(service.packet, packet, packet_length) == 0);
    memset(packet, 0, sizeof packet); /* Service owns its queued request. */
    assert(!h9_aprs_tx_service_next_sample(&service, &sample));
    assert(h9_aprs_tx_service_step(&service, true, false, false, 255) ==
           H9_APRS_TX_WAITING);
    assert(h9_aprs_tx_service_step(&service, false, false, false, 29) ==
           H9_APRS_TX_ENCODING);
    while (samples < 20000 && h9_aprs_tx_service_next_sample(&service, &sample)) {
        (void)h9_aprs_demod_sample(&demodulator, (int16_t)sample - 128);
        while (h9_aprs_demod_pop_byte(&demodulator, &receive_byte))
            (void)h9_aprs_frame_feed(&receive_frame, receive_byte,
                                     emit_packet, &decoded);
        samples++;
    }
    assert(samples >= 5376 && samples < 20000);
    assert(service.phase == H9_APRS_TX_IDLE);
    assert(decoded.packets == 1 && decoded.length == packet_length);
    assert(memcmp(decoded.bytes, expected, packet_length) == 0);
    assert(h9_aprs_tx_service_submit(&service, packet, packet_length));
    assert(h9_aprs_tx_service_step(&service, true, true, true, 255) ==
           H9_APRS_TX_IDLE);
    assert(!h9_aprs_tx_service_next_sample(&service, &sample));
}

static void test_aprs_controller(void)
{
    FakeHardware fake = { 0 };
    H9AprsHardware hw = { 0 };
    H9AprsController controller;
    H9MainAprsVersions versions;
    H9MainWireReceiver main_receiver;
    uint8_t send_packet[] = { 4, 0x41, 0xC0, 0xDB };
    uint8_t query = 1;
    unsigned i;
    hw.context = &fake;
    hw.uart_write = uart_write;
    hw.queue_packet_tx = queue_packet;
    hw.transmit_aprs_packet = transmit_afsk_packet;
    hw.read_adc = read_adc;
    hw.demod_sample = demod;
    hw.read_dac_port = read_dac;
    hw.write_dac_port = write_dac;
    hw.tx_wave_active = tx_active;
    hw.next_tx_wave_sample = next_tx_wave;
    h9_aprs_controller_init(&controller, &hw);
    feed_frame(&controller, 0x00u, send_packet, sizeof send_packet);
    assert(fake.afsk_packet_length == sizeof send_packet);
    assert(memcmp(fake.afsk_packet, send_packet,
                  sizeof send_packet) == 0);
    feed_frame(&controller, 0x11u, send_packet, sizeof send_packet);
    assert(fake.packet_length == 3);
    assert(memcmp(fake.packet, send_packet + 1, 3) == 0);
    feed_frame(&controller, 0x20u, &query, 1);
    assert(fake.uart_length == 12);
    assert(fake.uart[0] == 0xC0u && fake.uart[1] == 0x21u);
    assert(fake.uart[2] == 1 && memcmp(fake.uart + 3, "V1.0.15", 7) == 0);
    h9_main_wire_reset(&main_receiver);
    for (i = 0; i < fake.uart_length - 1; ++i)
        assert(h9_main_wire_feed(&main_receiver, fake.uart[i]) ==
               H9_MAIN_WIRE_MORE);
    assert(h9_main_wire_feed(&main_receiver, fake.uart[i]) ==
           H9_MAIN_WIRE_FRAME);
    assert(main_receiver.command == 0x21u && main_receiver.length == 9u);
    assert(main_receiver.payload[0] == 1u);
    assert(memcmp(main_receiver.payload + 1, "V1.0.15", 7) == 0);
    assert(main_receiver.payload[8] == 0);
    h9_main_aprs_versions_init(&versions);
    assert(h9_main_aprs_version_from_frame(&main_receiver, &versions));
    assert(versions.has_module && !versions.has_protocol);
    assert(strcmp(versions.module, "V1.0.15") == 0);
    query = 0;
    feed_frame(&controller, 0x20u, &query, 1);
    h9_main_wire_reset(&main_receiver);
    for (i = 0; i < fake.uart_length; ++i)
        (void)h9_main_wire_feed(&main_receiver, fake.uart[i]);
    assert(main_receiver.frame_ready);
    assert(h9_main_aprs_version_from_frame(&main_receiver, &versions));
    assert(versions.has_module && versions.has_protocol);
    assert(strcmp(versions.module, "V1.0.15") == 0);
    assert(strcmp(versions.protocol, "V1.0.0") == 0);
    main_receiver.length = 2;
    main_receiver.payload[0] = 1;
    main_receiver.payload[1] = 'X';
    assert(!h9_main_aprs_version_from_frame(&main_receiver, &versions));
    assert(strcmp(versions.module, "V1.0.15") == 0);
    h9_aprs_controller_adc_tick(&controller);
    assert(fake.demod_calls == 1 && fake.sample == 0);
    assert(fake.dac_calls == 1 && fake.dac == 0x85u);
    fake.tx_active = true;
    fake.dac_calls = 0;
    h9_aprs_controller_adc_tick(&controller);
    assert(fake.demod_calls == 2 && fake.dac == 0x45u);
    controller.hardware.read_dac_port = NULL;
    controller.hardware.write_dac_port = NULL;
    fake.dac_calls = 0;
    h9_aprs_controller_adc_tick(&controller);
    assert(fake.demod_calls == 3 && fake.dac_calls == 0);

    controller.hardware.read_dac_port = read_dac;
    controller.hardware.write_dac_port = write_dac;
    controller.hardware.tx_wave_active = NULL;
    controller.hardware.next_tx_wave_sample = NULL;
    assert(h9_aprs_controller_configure_tx(&controller, 0, 0));
    assert(h9_aprs_controller_queue_tx_byte(&controller, 0xFFu));
    h9_aprs_controller_adc_tick(&controller);
    assert(fake.demod_calls == 4 && fake.dac_calls == 1 &&
           fake.dac == 0xB5u);

    fake.uart_length = 0;
    assert(h9_aprs_ring_push(&controller.demod.receive, 0x7Eu));
    for (i = 0; i < 18; ++i)
        assert(h9_aprs_ring_push(&controller.demod.receive, (uint8_t)i));
    assert(h9_aprs_ring_push(&controller.demod.receive, 0x45u));
    assert(h9_aprs_ring_push(&controller.demod.receive, 0x87u));
    assert(h9_aprs_ring_push(&controller.demod.receive, 0x7Eu));
    h9_aprs_controller_process_received(&controller);
    assert(fake.uart_length == 21);
    assert(fake.uart[0] == 0xC0u && fake.uart[1] == 0 &&
           fake.uart[20] == 0xC0u);
    for (i = 0; i < 18; ++i) assert(fake.uart[i + 2] == (uint8_t)i);
}

int main(void)
{
    test_wire_vectors();
    test_main_aprs_wire_envelope();
    test_main_aprs_wire_receiver();
    test_main_ax25_packet();
    test_main_received_aprs_packet();
    test_main_gps_sentence_frames();
    test_main_gpio_query_reply();
    test_main_policy();
    test_timed_beacon_scheduler();
    test_recovered_bit_path();
    test_aprs_byte_queue();
    test_stock_waveform_vectors();
    test_stock_demod_delay();
    test_stock_tx_scheduler();
    test_aprs_frame_fcs();
    test_aprs_tx_packet_encoder();
    test_aprs_tx_gate();
    test_aprs_tx_service();
    test_aprs_controller();
    return 0;
}
