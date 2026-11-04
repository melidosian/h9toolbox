#include "h9_aprs_controller.h"
#include "h9_aprs_adc_board.h"
#include "h9_aprs_demod.h"
#include "h9_aprs_frame.h"
#include "h9_aprs_gpio_board.h"
#include "h9_aprs_bitstream.h"
#include "h9_aprs_clock.h"
#include "h9_aprs_ring.h"
#include "h9_aprs_sample_timer.h"
#include "h9_aprs_status.h"
#include "h9_aprs_systick.h"
#include "h9_aprs_tx.h"
#include "h9_aprs_tx_gate.h"
#include "h9_aprs_tx_packet.h"
#include "h9_aprs_tx_service.h"
#include "h9_aprs_tx_runtime.h"
#include "h9_aprs_uart_receive.h"
#include "h9_aprs_uart1_dma.h"
#include "h9_aprs_uart1_board.h"
#include "h9_aprs_uart1_tx.h"
#include "h9_aprs_uart2_board.h"
#include "h9_aprs_uart2_rx.h"
#include "h9_aprs_oscillator.h"
#include "h9_aprs_wave_output.h"
#include "h9_main_aprs.h"
#include "h9_main_aprs_control.h"
#include "h9_main_aprs_metadata.h"
#include "h9_main_aprs_receive.h"
#include "h9_main_aprs_session.h"
#include "h9_main_aprs_wire.h"
#include "h9_main_ax25.h"
#include "h9_main_beacon.h"
#include "h9_main_boot_record.h"
#include "h9_main_serial_profile.h"
#include "h9_main_serial_dma.h"
#include "h9_main_uart1_irq.h"
#include "h9_main_uart1_tx.h"
#include "h9_main_uart1_tx_queue.h"
#include "h9_main_tx_service.h"
#include "h9_main_uart_ring.h"
#include <assert.h>
#include <string.h>

typedef struct {
    uint8_t uart[H9_APRS_ENCODED_CAPACITY];
    size_t uart_length;
    unsigned uart_writes;
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
    bool aux_gate;
    bool status_valid;
    H9AprsStatusSnapshot status;
} FakeHardware;

static void uart_write(void *opaque, const uint8_t *data, size_t length)
{
    FakeHardware *fake = (FakeHardware *)opaque;
    assert(length <= sizeof fake->uart - fake->uart_length);
    memcpy(fake->uart + fake->uart_length, data, length);
    fake->uart_length += length;
    fake->uart_writes++;
}

static void queue_packet(void *opaque, const uint8_t *data, size_t length)
{
    FakeHardware *fake = (FakeHardware *)opaque;
    assert(length <= sizeof fake->packet);
    memcpy(fake->packet, data, length);
    fake->packet_length = length;
}

static bool read_aux_gate(void *opaque)
{
    return ((FakeHardware *)opaque)->aux_gate;
}

static bool read_status(void *opaque, uint8_t output[H9_APRS_STATUS_BYTES])
{
    FakeHardware *fake = (FakeHardware *)opaque;
    if (!fake->status_valid) return false;
    h9_aprs_status_encode(&fake->status, output);
    return true;
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
static bool next_tx_wave(void *opaque, uint8_t *sample)
{
    (void)opaque;
    if (sample == NULL) return false;
    *sample = 0x20u;
    return true;
}
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

static void test_aprs_systick(void)
{
    uint32_t registers[3] = { 0xAAu, 0xBBu, 0xCCu };
    H9AprsSysTick timer;
    h9_aprs_systick_bind(&timer, registers);
    assert(!h9_aprs_systick_start(&timer, 24000000u, 0u));
    assert(!h9_aprs_systick_start(&timer, 24000000u, 3u));
    assert(registers[0] == 0xAAu && registers[1] == 0xBBu);
    assert(h9_aprs_systick_start(&timer, 24000000u, 1u));
    assert(registers[0] == 7u && registers[1] == 23999u &&
           registers[2] == 0u);
    h9_aprs_systick_irq(&timer);
    h9_aprs_systick_irq(&timer);
    assert(h9_aprs_systick_now(&timer) == 2u);
    assert(h9_aprs_systick_start(&timer, 24000000u, 10u));
    assert(registers[1] == 239999u);
    h9_aprs_systick_irq(&timer);
    assert(h9_aprs_systick_now(&timer) == 10u);
}

static void test_wire_stream_maximum(void)
{
    uint8_t payload[H9_APRS_MAX_SEND];
    uint8_t expected[H9_APRS_ENCODED_CAPACITY];
    FakeHardware fake = { 0 };
    size_t length;
    memset(payload, H9_APRS_FRAME_END, sizeof payload);
    length = h9_wire_encode(0x10u, payload, sizeof payload,
                            expected, sizeof expected);
    assert(length == sizeof expected);
    assert(h9_wire_encode_stream(0x10u, payload, sizeof payload,
                                 uart_write, &fake) == length);
    assert(fake.uart_length == length && fake.uart_writes > 1u);
    assert(memcmp(fake.uart, expected, length) == 0);
    assert(h9_wire_encode_stream(0x10u, NULL, 1u,
                                 uart_write, &fake) == 0u);
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

static void test_main_control_frames(void)
{
    static const uint8_t check[] = "123456789";
    static const H9MainControlAction supported[] = {
        H9_MAIN_CONTROL_06_01, H9_MAIN_CONTROL_06_02,
        H9_MAIN_CONTROL_06_03, H9_MAIN_CONTROL_06_04,
        H9_MAIN_CONTROL_06_05, H9_MAIN_CONTROL_06_06,
        H9_MAIN_CONTROL_0A_01, H9_MAIN_CONTROL_0A_03
    };
    H9MainWireReceiver receiver;
    H9MainControlAction action;
    uint8_t frame[H9_MAIN_APRS_WIRE_CAPACITY];
    uint8_t payload[5];
    uint16_t crc;
    size_t length;
    size_t index;
    size_t i;
    assert(h9_main_control_crc(check, sizeof check - 1u) == 0x29B1u);
    assert(h9_main_control_crc(NULL, 2u) == 0u);
    for (index = 0; index < sizeof supported / sizeof supported[0]; ++index) {
        payload[0] = (uint8_t)((unsigned)supported[index] >> 8);
        payload[1] = (uint8_t)supported[index];
        payload[2] = 0xC0u; /* Exercises the incoming wire escape path. */
        crc = h9_main_control_crc(payload, 3u);
        payload[3] = (uint8_t)(crc >> 8);
        payload[4] = (uint8_t)crc;
        length = h9_main_aprs_wire_encode(0x51u, payload, sizeof payload,
                                          frame, sizeof frame);
        assert(length > 0);
        h9_main_wire_reset(&receiver);
        for (i = 0; i < length; ++i) {
            if (h9_main_wire_feed(&receiver, frame[i]) == H9_MAIN_WIRE_FRAME)
                break;
        }
        assert(i < length);
        assert(receiver.frame_ready && receiver.command == 0x51u);
        assert(receiver.length == sizeof payload);
        assert(h9_main_control_from_frame(&receiver, &action));
        assert(action == supported[index]);
        receiver.payload[4] ^= 1u;
        assert(!h9_main_control_from_frame(&receiver, &action));
        assert(action == H9_MAIN_CONTROL_NONE);
    }
    receiver.payload[0] = 0x07u;
    receiver.payload[1] = 0x01u;
    crc = h9_main_control_crc(receiver.payload, 3u);
    receiver.payload[3] = (uint8_t)(crc >> 8);
    receiver.payload[4] = (uint8_t)crc;
    assert(!h9_main_control_from_frame(&receiver, &action));
    receiver.command = 0x10u;
    assert(!h9_main_control_from_frame(&receiver, &action));
    receiver.command = 0x51u;
    receiver.length = 3u;
    assert(!h9_main_control_from_frame(&receiver, &action));
    receiver.length = 5u;
    receiver.frame_ready = 0u;
    assert(!h9_main_control_from_frame(&receiver, &action));
}

typedef struct {
    unsigned calls;
    uint8_t command;
    uint8_t payload[4];
    size_t length;
} MainUartCapture;

static void capture_main_uart_frame(void *context,
                                    const H9MainWireReceiver *receiver)
{
    MainUartCapture *capture = (MainUartCapture *)context;
    assert(receiver->frame_ready && receiver->length <= sizeof capture->payload);
    capture->calls++;
    capture->command = receiver->command;
    capture->length = receiver->length;
    memcpy(capture->payload, receiver->payload, receiver->length);
}

static void test_main_uart_ring(void)
{
    H9MainUartRing ring;
    H9MainWireReceiver receiver;
    MainUartCapture capture = { 0 };
    h9_main_uart_ring_init(&ring);
    h9_main_wire_reset(&receiver);
    ring.read_index = 2046u;
    ring.write_index = 4u;
    ring.storage[2046] = 0xC0u;
    ring.storage[2047] = 0x21u;
    ring.storage[0] = 1u;
    ring.storage[1] = 'X';
    ring.storage[2] = 0u;
    ring.storage[3] = 0xC0u;
    assert(h9_main_uart_ring_drain(&ring, &receiver,
                                   capture_main_uart_frame, &capture) == 6u);
    assert(ring.read_index == 4u && capture.calls == 1u);
    assert(capture.command == 0x21u && capture.length == 3u);
    assert(capture.payload[0] == 1u && capture.payload[1] == 'X' &&
           capture.payload[2] == 0u);
    assert(h9_main_uart_ring_drain(&ring, &receiver, NULL, NULL) == 0u);
    ring.read_index = 0u;
    ring.write_index = 1500u;
    memset(ring.storage, 0, 1500u);
    assert(h9_main_uart_ring_drain(&ring, &receiver, NULL, NULL) == 1024u);
    assert(ring.read_index == 1024u);
    assert(h9_main_uart_ring_drain(&ring, &receiver, NULL, NULL) == 476u);
    assert(ring.read_index == 1500u);
    ring.write_index = 2048u;
    assert(h9_main_uart_ring_drain(&ring, &receiver, NULL, NULL) == 0u);
    h9_main_uart_ring_init(&ring);
    for (size_t i = 0; i < H9_MAIN_UART_RING_CAPACITY - 1u; ++i)
        assert(h9_main_uart_ring_push(&ring, (uint8_t)i));
    assert(!h9_main_uart_ring_push(&ring, 0x42u));
    assert(ring.dropped_bytes == 1u && ring.write_index == 2047u);
    assert(h9_main_uart_ring_drain(&ring, &receiver, NULL, NULL) == 1024u);
    assert(h9_main_uart_ring_push(&ring, 0x42u));
    assert(ring.write_index == 0u && ring.dropped_bytes == 1u);
    h9_main_uart_ring_init(&ring);
    ring.write_index = 2040u;
    {
        const uint8_t batch[] = { 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u };
        assert(!h9_main_uart_ring_push_batch(&ring, batch, sizeof batch));
        assert(ring.write_index == 2040u && ring.dropped_bytes == 8u);
        ring.read_index = 100u;
        assert(h9_main_uart_ring_push_batch(&ring, batch, sizeof batch));
        assert(ring.write_index == 0u && ring.dropped_bytes == 8u);
        assert(memcmp(ring.storage + 2040u, batch, sizeof batch) == 0);
    }
}

typedef struct {
    unsigned packet_count;
    unsigned gps_count;
    unsigned control_count;
    unsigned version_count;
    unsigned serial_count;
    H9MainControlAction action;
    uint8_t source[7];
    uint8_t sentence[64];
    size_t sentence_length;
    uint8_t serial[16];
    size_t serial_length;
    uint8_t incoming[64];
    size_t incoming_length;
    size_t incoming_offset;
    size_t incoming_chunk;
    unsigned serial_read_count;
    bool invalid_read_length;
} MainSessionCapture;

static void session_packet(void *context, const H9MainAx25Decoded *packet)
{
    MainSessionCapture *capture = (MainSessionCapture *)context;
    capture->packet_count++;
    memcpy(capture->source, packet->source, sizeof capture->source);
}

static void session_gps(void *context, const uint8_t *sentence, size_t length)
{
    MainSessionCapture *capture = (MainSessionCapture *)context;
    assert(length <= sizeof capture->sentence);
    capture->gps_count++;
    capture->sentence_length = length;
    memcpy(capture->sentence, sentence, length);
}

static void session_control(void *context, H9MainControlAction action)
{
    MainSessionCapture *capture = (MainSessionCapture *)context;
    capture->control_count++;
    capture->action = action;
}

static void session_version(void *context, const H9MainAprsVersions *versions)
{
    MainSessionCapture *capture = (MainSessionCapture *)context;
    assert(versions->has_module && strcmp(versions->module, "1.0.15") == 0);
    capture->version_count++;
}

static void session_serial(void *context, const uint8_t *bytes, size_t length)
{
    MainSessionCapture *capture = (MainSessionCapture *)context;
    assert(length <= sizeof capture->serial);
    capture->serial_count++;
    capture->serial_length = length;
    memcpy(capture->serial, bytes, length);
}

static size_t session_serial_read(void *context, uint8_t *destination,
                                  size_t capacity)
{
    MainSessionCapture *capture = (MainSessionCapture *)context;
    size_t length = capture->incoming_length - capture->incoming_offset;
    assert(capacity == H9_MAIN_UART_DRAIN_LIMIT);
    capture->serial_read_count++;
    if (capture->invalid_read_length) return capacity + 1u;
    if (length > capture->incoming_chunk) length = capture->incoming_chunk;
    if (length > capacity) length = capacity;
    memcpy(destination, capture->incoming + capture->incoming_offset, length);
    capture->incoming_offset += length;
    return length;
}

static void queue_session_bytes(H9MainAprsSession *session,
                                const uint8_t *bytes, size_t length)
{
    size_t index;
    for (index = 0; index < length; ++index) {
        assert(h9_main_uart_ring_push(&session->uart_ring, bytes[index]));
    }
}

static void test_main_aprs_session(void)
{
    static const uint8_t info[] = { '!', 'O', 'K' };
    static const uint8_t gps_first[] = "$GPR";
    static const uint8_t gps_last[] = "MC,1*00\r\n";
    static const uint8_t version[] = { 1u, '1', '.', '0', '.', '1', '5', 0u };
    static const uint8_t gpio_query[] = { 3u };
    H9MainAprsSession session;
    H9MainAprsSessionCallbacks callbacks = { 0 };
    MainSessionCapture capture = { 0 };
    H9MainAx25Packet packet = { 0 };
    uint8_t raw[H9_MAIN_AX25_MAX_PACKET];
    uint8_t serial[H9_MAIN_APRS_WIRE_CAPACITY];
    uint8_t control[4];
    uint16_t crc;
    size_t raw_length;
    size_t serial_length;
    callbacks.packet = session_packet;
    callbacks.gps_sentence = session_gps;
    callbacks.control = session_control;
    callbacks.version = session_version;
    callbacks.serial_write = session_serial;
    h9_main_aprs_session_init(&session, &callbacks, &capture);
    memcpy(packet.destination, "APRS", 4u);
    memcpy(packet.source, "KD9CRQ", 6u);
    packet.information = info;
    packet.information_length = sizeof info;
    raw_length = h9_main_ax25_encode(&packet, raw, sizeof raw);
    assert(raw_length > 0u);
    serial_length = h9_wire_encode(0u, raw, raw_length,
                                   serial, sizeof serial);
    queue_session_bytes(&session, serial, serial_length / 2u);
    assert(h9_main_aprs_session_poll(&session) == serial_length / 2u);
    assert(capture.packet_count == 0u);
    queue_session_bytes(&session, serial + serial_length / 2u,
                        serial_length - serial_length / 2u);
    assert(h9_main_aprs_session_poll(&session) ==
           serial_length - serial_length / 2u);
    assert(capture.packet_count == 1u &&
           memcmp(capture.source, "KD9CRQ", 6u) == 0);

    serial_length = h9_wire_encode(0x10u, gps_first, sizeof gps_first - 1u,
                                   serial, sizeof serial);
    queue_session_bytes(&session, serial, serial_length);
    assert(h9_main_aprs_session_poll(&session) == serial_length);
    serial_length = h9_wire_encode(0x10u, gps_last, sizeof gps_last - 1u,
                                   serial, sizeof serial);
    queue_session_bytes(&session, serial, serial_length);
    assert(h9_main_aprs_session_poll(&session) == serial_length);
    assert(capture.gps_count == 1u &&
           capture.sentence_length == sizeof "$GPRMC,1*00\r\n" - 1u &&
           memcmp(capture.sentence, "$GPRMC,1*00\r\n",
                  capture.sentence_length) == 0);

    serial_length = h9_wire_encode(0x21u, version, sizeof version,
                                   serial, sizeof serial);
    queue_session_bytes(&session, serial, serial_length);
    assert(h9_main_aprs_session_poll(&session) == serial_length);
    assert(capture.version_count == 1u);

    control[0] = 0x06u;
    control[1] = 0x01u;
    crc = h9_main_control_crc(control, 2u);
    control[2] = (uint8_t)(crc >> 8);
    control[3] = (uint8_t)crc;
    serial_length = h9_wire_encode(0x51u, control, sizeof control,
                                   serial, sizeof serial);
    queue_session_bytes(&session, serial, serial_length);
    assert(h9_main_aprs_session_poll(&session) == serial_length);
    assert(capture.control_count == 1u &&
           capture.action == H9_MAIN_CONTROL_06_01);
    control[3] ^= 1u;
    serial_length = h9_wire_encode(0x51u, control, sizeof control,
                                   serial, sizeof serial);
    queue_session_bytes(&session, serial, serial_length);
    assert(h9_main_aprs_session_poll(&session) == serial_length);
    assert(capture.control_count == 1u);

    serial_length = h9_wire_encode(0x11u, gpio_query, sizeof gpio_query,
                                   serial, sizeof serial);
    queue_session_bytes(&session, serial, serial_length);
    assert(h9_main_aprs_session_poll(&session) == serial_length);
    assert(capture.serial_count == 1u && capture.serial_length == 11u);
    assert(h9_main_aprs_session_poll(&session) == 0u);

    /* Exercise the stock-sized board read through the ring and frame parser,
     * including a frame split across separate serial read calls. */
    serial_length = h9_wire_encode(0x21u, version, sizeof version,
                                   serial, sizeof serial);
    assert(serial_length <= sizeof capture.incoming);
    memcpy(capture.incoming, serial, serial_length);
    capture.incoming_length = serial_length;
    capture.incoming_chunk = serial_length / 2u;
    session.callbacks.serial_read = session_serial_read;
    assert(h9_main_aprs_session_poll(&session) == capture.incoming_chunk);
    assert(capture.version_count == 1u);
    capture.incoming_chunk = serial_length - capture.incoming_offset;
    assert(h9_main_aprs_session_poll(&session) == capture.incoming_chunk);
    assert(capture.version_count == 2u && capture.serial_read_count == 2u);
    capture.invalid_read_length = true;
    assert(h9_main_aprs_session_poll(&session) == 0u);
    assert(session.serial_read_faults == 1u && capture.version_count == 2u);

    capture.invalid_read_length = false;
    capture.incoming_offset = 0u;
    capture.incoming_length = 6u;
    capture.incoming_chunk = 6u;
    {
        static const uint8_t py_ready[] = {
            'P', 'Y', 2u, 1u, 0x55u, 0x01u
        };
        static const uint8_t py_query[] = {
            'P', 'Y', 2u, 1u, 0x0Du, 0xB9u
        };
        static const uint8_t old_bad_checksum[] = {
            'P', 'Y', 2u, 1u, 0x55u, 0xB1u
        };
        static const uint8_t thd_reply[] = {
            'T', 'H', 'D', 0u, 1u, 19u, 20u
        };
        memcpy(capture.incoming, old_bad_checksum, sizeof old_bad_checksum);
        assert(h9_main_aprs_session_poll(&session) == sizeof old_bad_checksum);
        assert(!session.py_ready && capture.serial_count == 1u);
        capture.incoming_offset = 0u;
        memcpy(capture.incoming, py_ready, sizeof py_ready);
        assert(h9_main_aprs_session_poll(&session) == 0u);
        assert(session.py_ready && capture.serial_count == 1u);
        capture.incoming_offset = 0u;
        memcpy(capture.incoming, py_query, sizeof py_query);
        assert(h9_main_aprs_session_poll(&session) == 0u);
        assert(capture.serial_count == 2u &&
               capture.serial_length == sizeof thd_reply &&
               memcmp(capture.serial, thd_reply, sizeof thd_reply) == 0);
    }
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

typedef struct {
    H9Side current_side;
    H9Side prepared_side;
    uint32_t prepared_hz;
    bool fail_prepare;
    bool fail_send;
    bool fail_stop;
    bool wrong_readback_side;
    bool wrong_readback_hz;
    unsigned prepare_calls;
    unsigned readback_calls;
    unsigned send_calls;
    unsigned stop_calls;
    unsigned restore_calls;
    unsigned step;
} FakeMainTx;

static bool main_read_side(void *opaque, H9Side *side)
{
    FakeMainTx *fake = (FakeMainTx *)opaque;
    *side = fake->current_side;
    return true;
}

static bool main_prepare(void *opaque, H9Side side, uint32_t hz)
{
    FakeMainTx *fake = (FakeMainTx *)opaque;
    assert(fake->step == 0u);
    fake->step = 1u;
    fake->prepare_calls++;
    fake->prepared_side = side;
    fake->prepared_hz = hz;
    return !fake->fail_prepare;
}

static bool main_read_prepared(void *opaque, H9Side *side, uint32_t *hz)
{
    FakeMainTx *fake = (FakeMainTx *)opaque;
    assert(fake->step == 1u);
    fake->step = 2u;
    fake->readback_calls++;
    *side = fake->wrong_readback_side ? H9_SIDE_A : fake->prepared_side;
    *hz = fake->wrong_readback_hz ? 445390000u : fake->prepared_hz;
    return true;
}

static bool main_send_packet(void *opaque, const uint8_t *packet, size_t length)
{
    FakeMainTx *fake = (FakeMainTx *)opaque;
    assert(fake->step == 2u);
    assert(packet[0] == 0x82u && length == 1u);
    fake->step = 3u;
    fake->send_calls++;
    return !fake->fail_send;
}

static bool main_stop_tx(void *opaque)
{
    FakeMainTx *fake = (FakeMainTx *)opaque;
    assert(fake->step == 1u || fake->step == 2u || fake->step == 3u);
    fake->step = 4u;
    fake->stop_calls++;
    return !fake->fail_stop;
}

static bool main_restore_receive(void *opaque, H9Side previous_side)
{
    FakeMainTx *fake = (FakeMainTx *)opaque;
    assert(fake->step == 4u);
    assert(previous_side == fake->current_side);
    fake->step = 5u;
    fake->restore_calls++;
    return true;
}

static void test_main_tx_service(void)
{
    const uint8_t packet[] = { 0x82u };
    const H9MainTxHardware hardware = {
        main_read_side, main_prepare, main_read_prepared,
        main_send_packet, main_stop_tx, main_restore_receive
    };
    H9MainAprsConfig cfg = { true, false, H9_RX_B, H9_TX_B_LOCK };
    FakeMainTx fake = { 0 };
    H9MainTxResult result;

    fake.current_side = H9_SIDE_A;
    result = h9_main_tx_send(&cfg, 445390000u, 144390000u,
                             packet, sizeof packet, &hardware, &fake);
    assert(result == H9_MAIN_TX_REJECTED && fake.prepare_calls == 0u);

    memset(&fake, 0, sizeof fake);
    fake.current_side = H9_SIDE_B;
    result = h9_main_tx_send(&cfg, 445390000u, 144390000u,
                             packet, sizeof packet, &hardware, &fake);
    assert(result == H9_MAIN_TX_SENT && fake.step == 5u);
    assert(fake.prepared_side == H9_SIDE_B && fake.prepared_hz == 144390000u);
    assert(fake.send_calls == 1u && fake.stop_calls == 1u &&
           fake.restore_calls == 1u);

    memset(&fake, 0, sizeof fake);
    fake.current_side = H9_SIDE_B;
    fake.fail_prepare = true;
    result = h9_main_tx_send(&cfg, 445390000u, 144390000u,
                             packet, sizeof packet, &hardware, &fake);
    assert(result == H9_MAIN_TX_PREPARE_FAILED && fake.send_calls == 0u);
    assert(fake.stop_calls == 1u && fake.restore_calls == 1u);

    memset(&fake, 0, sizeof fake);
    fake.current_side = H9_SIDE_B;
    fake.wrong_readback_side = true;
    result = h9_main_tx_send(&cfg, 445390000u, 144390000u,
                             packet, sizeof packet, &hardware, &fake);
    assert(result == H9_MAIN_TX_READBACK_FAILED && fake.send_calls == 0u);
    assert(fake.stop_calls == 1u && fake.restore_calls == 1u);

    memset(&fake, 0, sizeof fake);
    fake.current_side = H9_SIDE_B;
    fake.wrong_readback_hz = true;
    result = h9_main_tx_send(&cfg, 445390000u, 144390000u,
                             packet, sizeof packet, &hardware, &fake);
    assert(result == H9_MAIN_TX_READBACK_FAILED && fake.send_calls == 0u);
    assert(fake.stop_calls == 1u && fake.restore_calls == 1u);

    memset(&fake, 0, sizeof fake);
    fake.current_side = H9_SIDE_B;
    fake.fail_send = true;
    result = h9_main_tx_send(&cfg, 445390000u, 144390000u,
                             packet, sizeof packet, &hardware, &fake);
    assert(result == H9_MAIN_TX_SEND_FAILED && fake.send_calls == 1u);
    assert(fake.stop_calls == 1u && fake.restore_calls == 1u);

    memset(&fake, 0, sizeof fake);
    fake.current_side = H9_SIDE_B;
    fake.fail_stop = true;
    result = h9_main_tx_send(&cfg, 445390000u, 144390000u,
                             packet, sizeof packet, &hardware, &fake);
    assert(result == H9_MAIN_TX_CLEANUP_FAILED && fake.restore_calls == 1u);

    memset(&fake, 0, sizeof fake);
    fake.current_side = H9_SIDE_A;
    cfg.tx_choice = H9_TX_B_AUTO;
    result = h9_main_tx_send(&cfg, 445390000u, 144390000u,
                             packet, sizeof packet, &hardware, &fake);
    assert(result == H9_MAIN_TX_SENT && fake.prepared_side == H9_SIDE_B);
    assert(fake.restore_calls == 1u && fake.current_side == H9_SIDE_A);
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

typedef struct {
    H9AprsTxService service;
    H9AprsTxRuntime runtime;
    unsigned sample_calls;
    uint8_t dac;
} FakeTxRuntime;

static void runtime_submit(void *context, const uint8_t *packet, size_t length)
{
    FakeTxRuntime *fake = (FakeTxRuntime *)context;
    assert(h9_aprs_tx_runtime_submit(&fake->runtime, packet, length));
}

static bool runtime_wave_active(void *context)
{
    FakeTxRuntime *fake = (FakeTxRuntime *)context;
    return h9_aprs_tx_runtime_wave_active(&fake->runtime);
}

static bool runtime_next_wave(void *context, uint8_t *sample)
{
    FakeTxRuntime *fake = (FakeTxRuntime *)context;
    fake->sample_calls++;
    return h9_aprs_tx_runtime_next_sample(&fake->runtime, sample);
}

static uint8_t runtime_read_dac(void *context)
{
    (void)context;
    return 0u;
}

static void runtime_write_dac(void *context, uint8_t value)
{
    FakeTxRuntime *fake = (FakeTxRuntime *)context;
    fake->dac = value;
}

static void test_aprs_tx_runtime(void)
{
    FakeTxRuntime fake = { 0 };
    H9AprsController controller;
    H9AprsHardware hardware = { 0 };
    const uint8_t packet[18] = { 0x82u, 0xA0u, 0xA4u, 0xA6u };
    h9_aprs_tx_service_init(&fake.service, 0u, 0u, 0u);
    h9_aprs_tx_runtime_bind(&fake.runtime, &fake.service);
    hardware.context = &fake;
    hardware.transmit_aprs_packet = runtime_submit;
    hardware.tx_wave_active = runtime_wave_active;
    hardware.next_tx_wave_sample = runtime_next_wave;
    hardware.read_adc = read_adc;
    hardware.read_dac_port = runtime_read_dac;
    hardware.write_dac_port = runtime_write_dac;
    h9_aprs_controller_init(&controller, &hardware);
    assert(!h9_aprs_tx_runtime_submit(&fake.runtime, packet, sizeof packet));
    assert(h9_aprs_tx_runtime_step(&fake.runtime, false, true,
                                   false, false, 255u) == H9_APRS_TX_IDLE);
    assert(h9_aprs_tx_runtime_step(&fake.runtime, true, true,
                                   false, false, 255u) == H9_APRS_TX_IDLE);
    feed_frame(&controller, 0u, packet, sizeof packet);
    assert(fake.service.phase == H9_APRS_TX_WAITING);
    h9_aprs_controller_adc_tick(&controller);
    assert(fake.dac == 0x80u && fake.sample_calls == 0u);
    assert(h9_aprs_tx_runtime_step(&fake.runtime, true, true,
                                   false, false, 255u) == H9_APRS_TX_WAITING);
    assert(!h9_aprs_tx_runtime_wave_active(&fake.runtime));
    assert(h9_aprs_tx_runtime_step(&fake.runtime, true, true,
                                   false, false, 255u) == H9_APRS_TX_WAITING);
    assert(h9_aprs_tx_runtime_step(&fake.runtime, true, false,
                                   false, false, 255u) == H9_APRS_TX_DRAINING);
    assert(h9_aprs_tx_runtime_wave_active(&fake.runtime));
    h9_aprs_controller_adc_tick(&controller);
    assert(fake.sample_calls == 1u);
    assert(h9_aprs_tx_runtime_step(&fake.runtime, false, true,
                                   false, false, 255u) == H9_APRS_TX_IDLE);
    assert(fake.service.wave.active == 0u);
    h9_aprs_controller_adc_tick(&controller);
    assert(fake.dac == 0x80u && fake.sample_calls == 1u);
    assert(!h9_aprs_tx_runtime_submit(&fake.runtime, packet, sizeof packet));
    assert(h9_aprs_tx_runtime_step(&fake.runtime, true, true,
                                   false, false, 255u) == H9_APRS_TX_IDLE);
    feed_frame(&controller, 0u, packet, sizeof packet);
    assert(fake.service.phase == H9_APRS_TX_WAITING);
    assert(h9_aprs_tx_runtime_step(&fake.runtime, false, true,
                                   false, false, 255u) == H9_APRS_TX_IDLE);
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
    hw.aux_uart_write = queue_packet;
    hw.read_radio_gpio = read_aux_gate;
    hw.read_status = read_status;
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
    assert(fake.packet_length == 0u);
    fake.aux_gate = true;
    feed_frame(&controller, 0x11u, send_packet, sizeof send_packet);
    assert(fake.packet_length == 3);
    assert(memcmp(fake.packet, send_packet + 1, 3) == 0);
    assert(fake.uart_length == 0u);
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
    fake.uart_length = 0;
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
    fake.status_valid = true;
    fake.status.words[0] = 0x01020304u;
    fake.status.words[8] = 0x1000u;
    fake.status.words[9] = 0xAABBCCDDu;
    fake.status.current_tick = 0x1234u;
    fake.status.word_10_low = 0xABu;
    fake.status.serial_halfword = 0x1234u;
    fake.status.serial_byte_a = 0x56u;
    fake.status.serial_byte_b = 0x78u;
    fake.uart_length = 0u;
    feed_frame(&controller, 0x13u, &query, 0u);
    h9_main_wire_reset(&main_receiver);
    for (i = 0; i < fake.uart_length; ++i)
        (void)h9_main_wire_feed(&main_receiver, fake.uart[i]);
    assert(main_receiver.frame_ready && main_receiver.command == 0x13u &&
           main_receiver.length == H9_APRS_STATUS_BYTES);
    assert(main_receiver.payload[0] == 4u &&
           main_receiver.payload[1] == 3u &&
           main_receiver.payload[2] == 2u &&
           main_receiver.payload[3] == 1u);
    assert(main_receiver.payload[32] == 0u &&
           main_receiver.payload[33] == 0x10u &&
           main_receiver.payload[36] == 0xDDu &&
           main_receiver.payload[39] == 0xAAu);
    assert(main_receiver.payload[40] == 0xABu &&
           main_receiver.payload[41] == 0x34u &&
           main_receiver.payload[42] == 0x34u &&
           main_receiver.payload[43] == 0x12u &&
           main_receiver.payload[44] == 0x56u &&
           main_receiver.payload[45] == 0x78u &&
           main_receiver.payload[46] == 0x34u &&
           main_receiver.payload[47] == 0x02u);
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

typedef struct {
    unsigned calls;
    uint8_t *buffer;
    size_t capacity;
} AprsUartRearmCapture;

static void capture_aprs_uart_rearm(void *context, uint8_t *buffer,
                                    size_t capacity)
{
    AprsUartRearmCapture *capture = (AprsUartRearmCapture *)context;
    capture->calls++;
    capture->buffer = buffer;
    capture->capacity = capacity;
}

static void test_aprs_uart_receive_batches(void)
{
    H9AprsUartReceive receive;
    H9AprsController controller;
    H9AprsHardware hardware = { 0 };
    FakeHardware fake = { 0 };
    AprsUartRearmCapture rearm = { 0 };
    uint8_t *buffer;
    hardware.context = &fake;
    hardware.uart_write = uart_write;
    h9_aprs_controller_init(&controller, &hardware);
    h9_aprs_uart_receive_init(&receive);
    buffer = h9_aprs_uart_receive_buffer(&receive);
    assert(buffer == receive.buffers[0]);
    buffer[0] = 0xC0u;
    buffer[1] = 0x20u;
    assert(!h9_aprs_uart_receive_idle(&receive, 513u,
                                      capture_aprs_uart_rearm, &rearm));
    assert(rearm.calls == 0u);
    assert(!h9_aprs_uart_receive_idle(&receive, 512u,
                                      capture_aprs_uart_rearm, &rearm));
    assert(rearm.calls == 1u && rearm.buffer == receive.buffers[0]);
    rearm.calls = 0;
    assert(!h9_aprs_uart_receive_complete(&receive, 0));
    assert(!h9_aprs_uart_receive_complete(&receive, 513));
    assert(h9_aprs_uart_receive_idle(&receive, 510u,
                                     capture_aprs_uart_rearm, &rearm));
    assert(rearm.calls == 0u);
    assert(h9_aprs_uart_receive_buffer(&receive) == NULL);
    assert(!h9_aprs_uart_receive_complete(&receive, 2));
    assert(!h9_aprs_uart_receive_idle(&receive, 510u,
                                      capture_aprs_uart_rearm, &rearm));
    assert(rearm.calls == 0u);
    assert(h9_aprs_uart_receive_process(&receive, &controller,
                                        capture_aprs_uart_rearm, &rearm) == 2u);
    assert(fake.uart_length == 0 && rearm.calls == 1u);
    assert(rearm.buffer == receive.buffers[1] && rearm.capacity == 512u);
    buffer = h9_aprs_uart_receive_buffer(&receive);
    buffer[0] = 1u;
    buffer[1] = 0xC0u;
    assert(h9_aprs_uart_receive_complete(&receive, 2));
    assert(h9_aprs_uart_receive_process(&receive, &controller,
                                        capture_aprs_uart_rearm, &rearm) == 2u);
    assert(fake.uart_length > 3 && fake.uart[0] == 0xC0u &&
           fake.uart[1] == 0x21u);
    assert(receive.current == 0u && rearm.calls == 2u &&
           rearm.buffer == receive.buffers[0]);
    assert(h9_aprs_uart_receive_idle(&receive, 0u, NULL, NULL));
    assert(receive.pending_length == 512u && receive.ready == 1u);
    h9_aprs_uart_receive_init(&receive);
    assert(h9_aprs_uart_receive_process(&receive, &controller, NULL, NULL) == 0);
}

static void test_aprs_uart1_dma_interrupts(void)
{
    H9AprsUartReceive receive;
    H9AprsUart1Dma driver;
    H9AprsController controller;
    uint32_t usart[6] = { 0 };
    uint32_t dma_controller[2] = { 0 };
    uint32_t dma_channel[4] = { 0 };
    h9_aprs_controller_init(&controller, NULL);
    h9_aprs_uart_receive_init(&receive);
    h9_aprs_uart1_dma_bind(&driver, usart, dma_controller, dma_channel,
                           &receive);
    assert(h9_aprs_uart1_dma_start(&driver));
    assert(dma_channel[0] == 0x308Bu && dma_channel[1] == 512u &&
           dma_channel[2] == (uint32_t)(uintptr_t)&usart[1] &&
           dma_channel[3] ==
               (uint32_t)(uintptr_t)receive.buffers[0]);
    assert((usart[3] & 0x10u) != 0 && (usart[5] & 0x40u) != 0);

    receive.buffers[0][0] = 0xC0u;
    receive.buffers[0][1] = 0x20u;
    dma_channel[1] = 510u;
    usart[0] = 0x10u;
    h9_aprs_uart1_irq(&driver);
    assert(receive.ready == 1u && receive.pending_length == 2u &&
           (dma_channel[0] & 1u) == 0 && (usart[5] & 0x40u) == 0);
    assert(h9_aprs_uart_receive_process(&receive, &controller,
            h9_aprs_uart1_dma_rearm, &driver) == 2u);
    assert(receive.current == 1u && dma_channel[1] == 512u &&
           dma_channel[3] ==
               (uint32_t)(uintptr_t)receive.buffers[1]);

    /* Empty UART IDLE immediately rearms the same buffer. */
    h9_aprs_uart1_irq(&driver);
    assert(receive.ready == 0u && receive.current == 1u &&
           dma_channel[1] == 512u && (dma_channel[0] & 1u) != 0);

    /* A full DMA buffer is published even without a following IDLE. */
    dma_controller[0] = 0x02u;
    h9_aprs_dma1_channel1_irq(&driver);
    assert(receive.ready == 1u && receive.pending_length == 512u &&
           dma_controller[1] == 0x0Fu);
    assert(h9_aprs_uart_receive_process(&receive, &controller,
            h9_aprs_uart1_dma_rearm, &driver) == 512u);

    dma_controller[0] = 0x08u;
    h9_aprs_dma1_channel1_irq(&driver);
    assert(receive.ready == 0u && dma_channel[1] == 512u &&
           (dma_channel[0] & 1u) != 0);
}

static void test_aprs_uart1_board_setup(void)
{
    H9AprsUart1Board board;
    uint32_t rcc[17] = { 0 };
    uint32_t gpio[9] = { 0 };
    uint32_t usart[6] = { 0 };
    uint32_t nvic_iser = 0;
    assert(h9_aprs_uart1_peripheral_clock(48000000u, 0) == 48000000u);
    assert(h9_aprs_uart1_peripheral_clock(48000000u, 0x4000u) == 24000000u);
    assert(h9_aprs_uart1_peripheral_clock(48000000u, 0x7000u) == 3000000u);
    assert(h9_aprs_uart1_brr(48000000u) == 417u);
    assert(h9_aprs_uart1_brr(24000000u) == 208u);
    assert(h9_aprs_uart1_brr(1000000u) == 0u);
    h9_aprs_uart1_board_bind(&board, rcc, gpio, usart, &nvic_iser);
    assert(!h9_aprs_uart1_board_configure(&board, 0));
    assert(rcc[14] == 0 && nvic_iser == 0);
    gpio[0] = gpio[1] = gpio[2] = gpio[3] = gpio[8] = ~0u;
    usart[3] = usart[4] = usart[5] = ~0u;
    assert(h9_aprs_uart1_board_configure(&board, 48000000u));
    assert((rcc[14] & 1u) != 0 && (rcc[13] & 2u) != 0 &&
           (rcc[16] & 0x4000u) != 0);
    assert(((gpio[0] >> 12) & 0xFu) == 0xAu &&
           ((gpio[1] >> 6) & 3u) == 0u &&
           ((gpio[2] >> 12) & 0xFu) == 0xFu &&
           ((gpio[3] >> 12) & 0xFu) == 0x5u &&
           ((gpio[8] >> 24) & 0xFFu) == 0u);
    assert(usart[2] == 417u && (usart[3] & 0x200Cu) == 0x200Cu &&
           (usart[4] & 0x2800u) == 0u &&
           (usart[5] & 0xB08u) == 0u);
    assert(nvic_iser == 0u);
    h9_aprs_uart1_board_enable_irqs(&board);
    assert(nvic_iser == ((1u << 9) | (1u << 27)));
}

static void test_aprs_uart2_board_setup_and_raw_write(void)
{
    H9AprsUart2Board board = { 0 };
    uint32_t rcc[17] = { 0 };
    uint32_t gpio[9] = { 0 };
    uint32_t usart[6] = { 0 };
    const uint8_t raw[] = { 0xC0u, 0xDBu };
    assert(h9_aprs_uart_brr(48000000u, 9600u) == 5000u);
    assert(h9_aprs_uart_brr(48000000u, 0u) == 0u);
    h9_aprs_uart2_board_bind(&board, rcc, gpio, usart, 2u);
    assert(!h9_aprs_uart2_board_configure(&board, 0u));
    gpio[0] = gpio[1] = gpio[2] = gpio[3] = gpio[8] = ~0u;
    usart[3] = usart[4] = usart[5] = ~0u;
    assert(h9_aprs_uart2_board_configure(&board, 48000000u));
    assert((rcc[13] & 0x20u) != 0u &&
           (rcc[16] & 0x20000u) != 0u);
    assert((gpio[0] & 0xFu) == 0xAu &&
           (gpio[1] & 3u) == 0u &&
           (gpio[2] & 0xFu) == 0xAu &&
           (gpio[3] & 0xFu) == 0x5u &&
           (gpio[8] & 0xFFu) == 0x44u);
    assert(usart[2] == 5000u &&
           (usart[3] & 0x200Cu) == 0x200Cu &&
           (usart[4] & 0x2800u) == 0u &&
           (usart[5] & 0xB08u) == 0u);
    usart[0] = 0xC0u;
    assert(h9_aprs_uart2_board_write(&board, raw, sizeof raw) == 2u);
    assert(usart[1] == 0xDBu && board.bytes_completed == 2u);
    usart[0] = 0u;
    assert(h9_aprs_uart2_board_write(&board, raw, sizeof raw) == 0u);
    assert(board.faulted && board.timeouts == 1u);
    assert(h9_aprs_uart2_board_write(&board, raw, sizeof raw) == 0u);
    assert(board.timeouts == 1u);
}

static void test_aprs_uart2_irq_forward(void)
{
    H9AprsUart2Rx rx = { 0 };
    H9AprsController controller;
    H9AprsHardware hardware = { 0 };
    H9WireDecoder decoded;
    FakeHardware fake = { 0 };
    uint32_t usart[6] = { 0 };
    uint32_t nvic_iser = 0u;
    size_t index;
    hardware.context = &fake;
    hardware.uart_write = uart_write;
    h9_aprs_controller_init(&controller, &hardware);
    h9_aprs_uart2_rx_bind(&rx, usart, &nvic_iser);
    assert(h9_aprs_uart2_rx_start(&rx));
    assert(nvic_iser == (1u << 28) &&
           (usart[3] & 0x30u) == 0x30u);

    usart[0] = 0x20u;
    usart[1] = 'G';
    h9_aprs_uart2_rx_irq(&rx);
    usart[0] = 0x30u;
    usart[1] = 'P';
    h9_aprs_uart2_rx_irq(&rx);
    assert(rx.ready && rx.length == 2u &&
           (usart[3] & 0x30u) == 0u);
    assert(h9_aprs_uart2_rx_poll(&rx, &controller));
    assert(!rx.ready && rx.length == 0u &&
           (usart[3] & 0x30u) == 0x30u);
    h9_wire_reset(&decoded);
    for (index = 0; index < fake.uart_length; ++index)
        (void)h9_wire_feed(&decoded, fake.uart[index]);
    assert(decoded.command == 0x10u && decoded.length == 2u &&
           decoded.payload[0] == 'G' && decoded.payload[1] == 'P');

    usart[0] = 0x20u;
    usart[1] = 'X';
    h9_aprs_uart2_rx_irq(&rx);
    usart[0] = 0x08u;
    h9_aprs_uart2_rx_irq(&rx);
    assert(rx.overrun_count == 1u && rx.length == 0u && !rx.ready);
    assert(!h9_aprs_uart2_rx_poll(&rx, &controller));

    fake.uart_length = 0u;
    for (index = 0; index < H9_APRS_MAX_SEND; ++index) {
        usart[0] = 0x20u;
        usart[1] = 'A';
        h9_aprs_uart2_rx_irq(&rx);
    }
    assert(rx.ready && rx.length == H9_APRS_MAX_SEND);
    assert(h9_aprs_uart2_rx_poll(&rx, &controller));
    assert(fake.uart_length == H9_APRS_MAX_SEND + 3u);
    assert(rx.length == 0u && !rx.ready);
}

static void test_aprs_gpio_board(void)
{
    H9AprsGpioBoard board = { 0 };
    uint32_t gpio[11] = { 0 };
    uint32_t rcc[14] = { 0 };
    assert(!h9_aprs_gpio_board_read(&board));
    assert(!h9_aprs_gpio_board_configure(&board));
    h9_aprs_gpio_board_write(&board, true);
    h9_aprs_gpio_board_bind(&board, rcc, gpio);
    gpio[0] = gpio[1] = gpio[2] = gpio[3] = ~0u;
    assert(h9_aprs_gpio_board_configure(&board));
    assert((rcc[13] & 2u) != 0u &&
           ((gpio[0] >> 10) & 3u) == 1u &&
           ((gpio[1] >> 5) & 1u) == 0u &&
           ((gpio[2] >> 10) & 3u) == 0u &&
           ((gpio[3] >> 10) & 3u) == 0u &&
           gpio[10] == 0x20u);
    h9_aprs_gpio_board_write(&board, true);
    assert(gpio[6] == 0x20u && gpio[10] == 0x20u);
    h9_aprs_gpio_board_write(&board, false);
    assert(gpio[10] == 0x20u);
    assert(!h9_aprs_gpio_board_read(&board));
    gpio[4] = 0x20u;
    assert(h9_aprs_gpio_board_read(&board));
    gpio[4] = 0x10u;
    assert(!h9_aprs_gpio_board_read(&board));
}

static void test_aprs_wave_output(void)
{
    H9AprsWaveOutput output = { 0 };
    uint32_t rcc[14] = { 0 };
    uint32_t gpio[6] = { 0 };
    assert(h9_aprs_wave_output_read(&output) == 0u);
    h9_aprs_wave_output_bind(&output, rcc, gpio);
    gpio[0] = gpio[1] = gpio[2] = gpio[3] = ~0u;
    h9_aprs_wave_output_configure(&output);
    assert(rcc[13] == 1u);
    assert(((gpio[0] >> 8) & 0xFFu) == 0x55u);
    assert((gpio[1] & 0xF0u) == 0u);
    assert(((gpio[2] >> 8) & 0xFFu) == 0xFFu);
    assert(((gpio[3] >> 8) & 0xFFu) == 0x55u);
    gpio[5] = 0x18Fu;
    assert(h9_aprs_wave_output_read(&output) == 0x8Fu);
    h9_aprs_wave_output_write(&output, 0xA5u);
    assert(gpio[5] == 0xA5u);
}

typedef struct {
    uint32_t *adc;
    unsigned polls;
} FakeAdcCalibration;

static void adc_calibration_poll(void *context)
{
    FakeAdcCalibration *fake = (FakeAdcCalibration *)context;
    assert((fake->adc[3] & 3u) == 0u);
    fake->polls++;
    if (fake->polls == 2u) fake->adc[2] &= ~0x80000000u;
}

static void adc_ready_poll(void *context)
{
    FakeAdcCalibration *fake = (FakeAdcCalibration *)context;
    fake->polls++;
    if (fake->polls == 2u) fake->adc[0] |= 1u;
}

static void test_aprs_adc_board(void)
{
    H9AprsAdcBoard board = { 0 };
    FakeAdcCalibration fake;
    uint32_t rcc[17] = { 0 };
    uint32_t gpio[4] = { 0 };
    uint32_t adc[17] = { 0 };
    uint32_t nvic_iser = 0u;
    assert(h9_aprs_adc_board_read(&board) == 0u);
    h9_aprs_adc_board_bind(&board, rcc, gpio, adc, &nvic_iser);
    assert(h9_aprs_adc_board_reset_enable(&board));
    assert((rcc[12] & (1u << 20)) == 0u &&
           (rcc[16] & (1u << 20)) != 0u);
    adc[3] = 0xA5u;
    fake.adc = adc;
    fake.polls = 0u;
    assert(h9_aprs_adc_board_calibrate(&board, 4u,
                                       adc_calibration_poll, &fake));
    assert(fake.polls == 2u && adc[3] == 0xA5u);
    adc[2] = 0u;
    assert(!h9_aprs_adc_board_calibrate(&board, 2u, NULL, NULL));
    assert(adc[3] == 0xA5u);
    adc[2] = 1u;
    assert(!h9_aprs_adc_board_calibrate(&board, 2u,
                                        adc_calibration_poll, &fake));
    gpio[0] = gpio[3] = ~0u;
    h9_aprs_adc_board_prepare_pin(&board);
    assert(rcc[13] == 1u && rcc[16] == (1u << 20));
    assert(((gpio[0] >> 2) & 3u) == 3u);
    assert(((gpio[3] >> 2) & 3u) == 0u);
    adc[2] = 4u;
    assert(!h9_aprs_adc_board_configure(&board));
    adc[2] = 0u;
    adc[3] = adc[4] = adc[5] = adc[10] = 0u;
    assert(h9_aprs_adc_board_configure(&board));
    assert(adc[3] == 0x5C0Cu && adc[4] == 0x10000000u);
    assert(adc[5] == 7u && adc[10] == 2u);
    adc[0] = 0u;
    assert(!h9_aprs_adc_board_start(&board, 2u, NULL, NULL));
    assert(adc[2] == 1u && adc[1] == 0u && nvic_iser == 0u);
    adc[2] = 0u;
    fake.polls = 0u;
    assert(h9_aprs_adc_board_start(&board, 4u, adc_ready_poll, &fake));
    assert(fake.polls == 2u && adc[0] == 0x1Cu && adc[1] == 0x1Cu);
    assert(adc[2] == 5u && nvic_iser == (1u << 12));
    assert(!h9_aprs_adc_board_start(&board, 4u, adc_ready_poll, &fake));
    adc[16] = 0xABCD1234u;
    assert(h9_aprs_adc_board_read(&board) == 0x1234u);
}

static void test_aprs_sample_timer(void)
{
    H9AprsSampleTimer sample_timer = { 0 };
    uint32_t rcc[17] = { 0 };
    uint32_t timer[13] = { 0 };
    assert(!h9_aprs_sample_timer_start(&sample_timer));
    timer[0] = 0xFFFu;
    timer[1] = 0xFFu;
    timer[2] = 0xFFu;
    h9_aprs_sample_timer_bind(&sample_timer, rcc, timer);
    assert(h9_aprs_sample_timer_start(&sample_timer));
    assert(rcc[16] == 0x800u);
    assert(timer[0] == 0xC8Fu && timer[1] == 0xAFu &&
           timer[2] == 0x7Fu && timer[5] == 1u);
    assert(timer[10] == 11u && timer[11] == 209u && timer[12] == 0u);
}

typedef struct {
    H9AprsAdcBoard adc;
    H9AprsWaveOutput wave;
    unsigned demod_calls;
} FakeAprsSampleIrq;

static uint16_t sample_irq_read_adc(void *context)
{
    FakeAprsSampleIrq *fake = (FakeAprsSampleIrq *)context;
    return h9_aprs_adc_board_read(&fake->adc);
}

static uint8_t sample_irq_read_wave(void *context)
{
    FakeAprsSampleIrq *fake = (FakeAprsSampleIrq *)context;
    return h9_aprs_wave_output_read(&fake->wave);
}

static void sample_irq_write_wave(void *context, uint8_t value)
{
    FakeAprsSampleIrq *fake = (FakeAprsSampleIrq *)context;
    assert(fake->demod_calls == 1u);
    h9_aprs_wave_output_write(&fake->wave, value);
}

static void sample_irq_demod(void *context, int16_t normalized_sample)
{
    FakeAprsSampleIrq *fake = (FakeAprsSampleIrq *)context;
    assert(normalized_sample == 0);
    fake->demod_calls++;
}

static void test_aprs_adc_sample_irq(void)
{
    H9AprsController controller;
    H9AprsHardware hardware = { 0 };
    FakeAprsSampleIrq fake = { 0 };
    uint32_t adc[17] = { 0 };
    uint32_t rcc[17] = { 0 };
    uint32_t gpio[6] = { 0 };
    uint32_t nvic_iser = 0u;
    h9_aprs_adc_board_bind(&fake.adc, rcc, gpio, adc, &nvic_iser);
    h9_aprs_wave_output_bind(&fake.wave, rcc, gpio);
    hardware.context = &fake;
    hardware.read_adc = sample_irq_read_adc;
    hardware.read_dac_port = sample_irq_read_wave;
    hardware.write_dac_port = sample_irq_write_wave;
    hardware.demod_sample = sample_irq_demod;
    h9_aprs_controller_init(&controller, &hardware);
    adc[16] = 0x200u;
    gpio[5] = 0x1Au;
    adc[0] = 4u;
    assert(!h9_aprs_adc_board_sample_irq(&fake.adc, &controller));
    assert(fake.demod_calls == 0u && gpio[5] == 0x1Au);
    adc[1] = 8u;
    assert(!h9_aprs_adc_board_sample_irq(&fake.adc, &controller));
    adc[1] = 4u;
    assert(h9_aprs_adc_board_sample_irq(&fake.adc, &controller));
    assert(fake.demod_calls == 1u && gpio[5] == 0x8Au);
    assert(adc[0] == 0xCu);
}

static void test_aprs_internal_clock_selection(void)
{
    uint32_t rcc[3] = { 0 };
    assert(h9_aprs_clock_system_hz(0, 0x8000u, 0) == 24000000u);
    assert(h9_aprs_clock_system_hz(0x800u, 0x8000u, 0) == 12000000u);
    assert(h9_aprs_clock_system_hz(0, 0, 0x08u) == 24000000u);
    assert(h9_aprs_clock_system_hz(0, 0, 0x18u) == 32768u);
    assert(h9_aprs_clock_system_hz(0, 0, 0x10u) == 0u);
    assert(!h9_aprs_clock_select_internal(rcc, 0x1234u, 0));
    assert(!h9_aprs_clock_select_internal(rcc, 0x1234u, 2));
    assert(rcc[1] == 0x9234u && rcc[2] == 0u);
    rcc[0] = 0x3C00u;
    rcc[1] = 0xA5000000u;
    rcc[2] = 0x7F07u;
    assert(h9_aprs_clock_select_internal(rcc, 0x1234u, 2) == 24000000u);
    assert(rcc[0] == 0x400u && rcc[1] == 0xA5009234u &&
           rcc[2] == 0u);
    assert(h9_aprs_uart1_brr(
        h9_aprs_clock_system_hz(rcc[0], rcc[1], rcc[2])) == 208u);
    rcc[2] = 0x8u;
    assert(!h9_aprs_clock_select_internal(rcc, 0x1234u, 2));
}

static void test_aprs_uart1_reply_transmit(void)
{
    H9AprsUart1Tx tx;
    H9AprsController controller;
    H9AprsHardware hardware = { 0 };
    uint32_t usart[6] = { 0 };
    const uint8_t query[] = { 0xC0u, 0x20u, 2u, 0xC0u };
    const uint8_t bytes[] = { 0x31u, 0x32u, 0x33u };
    size_t index;
    h9_aprs_uart1_tx_bind(&tx, usart, 3u);
    usart[0] = 0xC0u;
    assert(h9_aprs_uart1_tx_write(&tx, bytes, sizeof bytes) == 3u);
    assert(tx.bytes_completed == 3u && tx.timeouts == 0u &&
           usart[1] == 0x33u);
    hardware.context = &tx;
    hardware.uart_write = h9_aprs_uart1_tx_callback;
    hardware.uart_ok = h9_aprs_uart1_tx_ok;
    h9_aprs_controller_init(&controller, &hardware);
    for (index = 0; index < sizeof query; ++index)
        h9_aprs_controller_uart_byte(&controller, query[index]);
    /* Command 0x20 query 2 replies with command 0x21, eight data bytes,
     * and two frame delimiters. */
    assert(tx.bytes_completed == 14u && usart[1] == 0xC0u);

    usart[0] = 0;
    assert(!h9_aprs_controller_forward_aux(&controller, bytes, 1u));
    assert(tx.timeouts == 1u && tx.faulted);
    usart[0] = 0xC0u;
    assert(h9_aprs_uart1_tx_write(&tx, bytes, 1u) == 0u);
    h9_aprs_uart1_tx_bind(&tx, usart, 3u);
    assert(h9_aprs_uart1_tx_write(&tx, bytes, 1u) == 1u);
    usart[0] = 0x80u;
    assert(h9_aprs_uart1_tx_write(&tx, bytes, 1u) == 0u);
    assert(tx.timeouts == 1u && tx.faulted && usart[1] == 0x31u);
}

static uint32_t boot_record_check(const volatile uint8_t *data,
                                  size_t length, void *context)
{
    uint32_t *checked = (uint32_t *)context;
    assert(length == H9_MAIN_BOOT_RECORD_CHECK_BYTES);
    assert(data[0] == 0x34u && data[1] == 0x12u);
    ++*checked;
    return 0x12345678u;
}

static uint32_t boot_record_check_updates_header(const volatile uint8_t *data,
                                                  size_t length, void *context)
{
    volatile uint8_t *header = (volatile uint8_t *)data - 2;
    (void)context;
    assert(length == H9_MAIN_BOOT_RECORD_CHECK_BYTES);
    header[0] = 0x78u;
    header[1] = 0x56u;
    return 0x12345678u;
}

static void test_main_boot_record(void)
{
    uint8_t record[H9_MAIN_BOOT_RECORD_BYTES];
    uint32_t checked = 0;
    size_t index;

    memset(record, 0xA5, sizeof record);
    record[0] = 0x78u; record[1] = 0x56u;
    record[2] = 0x34u; record[3] = 0x12u;
    record[4] = 0xCDu; record[5] = 0xABu;
    record[6] = 0xEFu; record[7] = 0xBEu;
    assert(h9_main_consume_boot_record(record, boot_record_check, &checked) ==
           0xBEEFABCDu);
    assert(checked == 1u);
    for (index = 0; index < sizeof record; ++index) assert(record[index] == 0u);

    memset(record, 0xA5, sizeof record);
    record[6] = 0xEFu; record[7] = 0xBEu;
    assert(h9_main_consume_boot_record(record, NULL, NULL) == 0xBEEF5A00u);
    for (index = 0; index < sizeof record; ++index) assert(record[index] == 0u);

    memset(record, 0xA5, sizeof record);
    record[2] = 0x34u; record[3] = 0x12u;
    record[6] = 0xEFu; record[7] = 0xBEu;
    assert(h9_main_consume_boot_record(record, boot_record_check, &checked) ==
           0xBEEF5A00u);
    assert(checked == 2u);
    for (index = 0; index < sizeof record; ++index) assert(record[index] == 0u);

    memset(record, 0xA5, sizeof record);
    record[2] = 0x34u; record[3] = 0x12u;
    record[4] = 0xCDu; record[5] = 0xABu;
    record[6] = 0xEFu; record[7] = 0xBEu;
    assert(h9_main_consume_boot_record(record,
                                      boot_record_check_updates_header, NULL) ==
           0xBEEFABCDu);
    for (index = 0; index < sizeof record; ++index) assert(record[index] == 0u);
}

static void test_main_serial_profile(void)
{
    H9MainSerialOpenRecord record;
    H9MainSerialRoute route = { 0 };

    h9_main_aprs_serial_profile(&record, 0x10CFCu, 0x01E78FFEu);
    assert(record.tx_pin == 0x20u && record.rx_pin == 0x21u);
    assert(record.storage_address == 0x10CFCu);
    assert(record.storage_size == 2048u && record.read_limit == 1024u);
    assert(record.field_10 == 10u);
    assert(record.event_callback_address == 0x01E78FFEu);
    assert(record.callback_context_address == 0u && record.baud == 19200u);
    assert(h9_main_serial_route_pins(record.tx_pin, record.rx_pin) == 1);
    assert(h9_main_serial_route(1, &route));
    assert(route.mmio_base == 0x001E2100u && route.interrupt_number == 0x13u);
    assert(h9_main_serial_route_pins(0xFFu, 0xFFu) == -1);
    assert(!h9_main_serial_route(3, &route));
}

static void test_main_serial_dma_read(void)
{
    uint8_t storage[2048] = { 0 };
    uint8_t copied[16] = { 0 };
    uint8_t serial[64];
    static const uint8_t version[] = { 1u, '1', '.', '0', '.', '1', '5', 0u };
    H9MainSerialDmaRing ring = { storage, sizeof storage, 2054u, 2044u };
    H9MainAprsSession session;
    H9MainAprsSessionCallbacks callbacks = { 0 };
    size_t index;
    size_t serial_length;

    for (index = 0u; index < 4u; ++index) storage[2044u + index] =
        (uint8_t)(0xA0u + index);
    for (index = 0u; index < 6u; ++index) storage[index] =
        (uint8_t)(0xA4u + index);
    assert(h9_main_serial_dma_read(&ring, copied, 6u) == 6u);
    for (index = 0u; index < 6u; ++index) assert(copied[index] == 0xA0u + index);
    assert(ring.consumed == 2050u);
    assert(h9_main_serial_dma_read(&ring, copied, sizeof copied) == 4u);
    for (index = 0u; index < 4u; ++index) assert(copied[index] == 0xA6u + index);
    assert(ring.consumed == ring.produced);
    assert(h9_main_serial_dma_read(&ring, copied, sizeof copied) == 0u);
    ring.capacity = 2047u;
    assert(h9_main_serial_dma_read(&ring, copied, sizeof copied) == 0u);

    serial_length = h9_wire_encode(0x21u, version, sizeof version,
                                   serial, sizeof serial);
    assert(serial_length != 0u && serial_length < sizeof serial);
    ring.capacity = sizeof storage;
    ring.consumed = 2045u;
    ring.produced = ring.consumed + (uint32_t)serial_length;
    for (index = 0u; index < serial_length; ++index)
        storage[(2045u + index) & 2047u] = serial[index];
    callbacks.serial_read = h9_main_serial_dma_read;
    h9_main_aprs_session_init(&session, &callbacks, &ring);
    assert(h9_main_aprs_session_poll(&session) == serial_length);
    assert(session.versions.has_module &&
           strcmp(session.versions.module, "1.0.15") == 0);
    assert(ring.consumed == ring.produced);
}

typedef struct {
    unsigned events[3];
    unsigned event_count;
    unsigned tx_acks;
    unsigned rx_acks;
} MainUartIrqCapture;

static void main_uart_tx_ack(void *context)
{
    ((MainUartIrqCapture *)context)->tx_acks++;
}

static void main_uart_rx_ack(void *context)
{
    ((MainUartIrqCapture *)context)->rx_acks++;
}

static void main_uart_event(void *context, unsigned number)
{
    MainUartIrqCapture *capture = (MainUartIrqCapture *)context;
    assert(capture->event_count < 3u);
    capture->events[capture->event_count++] = number;
}

static void test_main_uart1_irq(void)
{
    uint32_t registers[11] = { 0 };
    uint8_t storage[2048] = { 0 };
    H9MainSerialDmaRing ring = { storage, sizeof storage, 100u, 0u };
    MainUartIrqCapture capture = { { 0 }, 0u, 0u, 0u };
    H9MainUart1Irq irq = { registers, &ring, 1024u,
                          main_uart_tx_ack, main_uart_rx_ack,
                          main_uart_event, &capture };

    registers[0] = 0x8004u | 0x4008u | 0x0820u;
    registers[1] = 1u;
    registers[10] = 5u;
    h9_main_uart1_irq(&irq);
    assert(capture.event_count == 3u && capture.events[0] == 1u &&
           capture.events[1] == 2u && capture.events[2] == 3u);
    assert(capture.tx_acks == 1u && capture.rx_acks == 2u);
    assert(ring.produced == 1129u);
    assert((registers[0] & 0x3480u) == 0x3480u);
    assert((registers[1] & 0x2000u) != 0u);

    registers[0] = 0x0820u;
    registers[10] = 0u;
    h9_main_uart1_irq(&irq);
    assert(capture.event_count == 3u && capture.rx_acks == 2u &&
           ring.produced == 1129u);
}

typedef struct {
    uint32_t *registers;
    unsigned steps[2];
    unsigned count;
} MainTxDmaCapture;

static void main_uart_dma_prepare(void *context, uint32_t descriptor)
{
    MainTxDmaCapture *capture = (MainTxDmaCapture *)context;
    assert(descriptor == 0x001E3B0u);
    assert(capture->registers[0] == 0u);
    capture->steps[capture->count++] = 1u;
}

static void main_uart_dma_enable(void *context, uint32_t descriptor,
                                  uint32_t option)
{
    MainTxDmaCapture *capture = (MainTxDmaCapture *)context;
    assert(descriptor == 0x001E3B0u && option == 0u);
    assert(capture->registers[0] == 0x2004u);
    assert(capture->registers[5] == 0x12345678u);
    assert(capture->registers[6] == 24u);
    capture->steps[capture->count++] = 2u;
}

static void test_main_uart1_tx(void)
{
    uint32_t registers[7] = { 0 };
    MainTxDmaCapture capture = { registers, { 0 }, 0u };
    H9MainUart1Tx tx = { registers, main_uart_dma_prepare,
                         main_uart_dma_enable, &capture };

    assert(h9_main_uart1_dma_start(&tx, 0x12345678u, 0u));
    assert(capture.count == 0u && registers[0] == 0u);
    assert(h9_main_uart1_dma_start(&tx, 0x12345678u, 24u));
    assert(capture.count == 2u && capture.steps[0] == 1u &&
           capture.steps[1] == 2u);
    assert(registers[0] == 0x2000u);
    tx.enable_dma = NULL;
    assert(h9_main_uart1_dma_start(&tx, 0x12345678u, 0u));
    assert(!h9_main_uart1_dma_start(&tx, 0x12345678u, 24u));
    assert(capture.count == 2u && registers[0] == 0x2000u);
}

typedef struct {
    uint32_t *registers;
    unsigned starts;
    uint32_t addresses[3];
    uint32_t lengths[3];
} MainTxQueueCapture;

static void main_tx_queue_prepare(void *context, uint32_t descriptor)
{
    (void)context;
    assert(descriptor == 0x001E3B0u);
}

static void main_tx_queue_enable(void *context, uint32_t descriptor,
                                  uint32_t option)
{
    MainTxQueueCapture *capture = (MainTxQueueCapture *)context;
    assert(descriptor == 0x001E3B0u && option == 0u);
    assert(capture->starts < 3u);
    capture->addresses[capture->starts] = capture->registers[5];
    capture->lengths[capture->starts] = capture->registers[6];
    capture->starts++;
}

static void test_main_uart1_tx_queue(void)
{
    uint32_t registers[7] = { 0 };
    uint8_t storage[2][H9_MAIN_APRS_WIRE_CAPACITY] = { { 0 }, { 0 } };
    H9MainUart1TxSlot slots[2] = {
        { storage[0], 0x10000u, sizeof storage[0], 0u },
        { storage[1], 0x11000u, sizeof storage[1], 0u }
    };
    MainTxQueueCapture capture = { registers, 0u, { 0 }, { 0 } };
    H9MainUart1Tx port = { registers, main_tx_queue_prepare,
                           main_tx_queue_enable, &capture };
    H9MainUart1TxQueue queue;
    H9MainAprsSession session;
    H9MainAprsSessionCallbacks callbacks = { 0 };
    uint8_t query_frame[16];
    static const uint8_t gpio_query[] = { 3u };
    size_t query_length;
    uint8_t first[] = { 1u, 2u, 3u };
    static const uint8_t second[] = { 4u, 5u };

    assert(h9_main_uart1_tx_queue_init(&queue, &port, slots, 2u));
    assert(h9_main_uart1_tx_queue_enqueue(&queue, first, sizeof first));
    assert(queue.active && queue.count == 1u && capture.starts == 1u);
    assert(capture.addresses[0] == 0x10000u &&
           capture.lengths[0] == sizeof first);
    first[0] = 99u;
    assert(slots[0].storage[0] == 1u);
    h9_main_uart1_tx_queue_write(&queue, second, sizeof second);
    assert(queue.count == 2u && capture.starts == 1u);
    assert(!h9_main_uart1_tx_queue_enqueue(&queue, first, sizeof first));
    assert(queue.rejected == 1u);
    h9_main_uart1_tx_queue_event(&queue, 2u);
    h9_main_uart1_tx_queue_pump(&queue);
    assert(queue.active && queue.count == 2u && capture.starts == 1u);
    h9_main_uart1_tx_queue_event(&queue, 1u);
    h9_main_uart1_tx_queue_pump(&queue);
    assert(queue.active && queue.count == 1u && capture.starts == 2u);
    assert(capture.addresses[1] == 0x11000u &&
           capture.lengths[1] == sizeof second);
    assert(slots[1].storage[0] == 4u);
    h9_main_uart1_tx_queue_event(&queue, 1u);
    h9_main_uart1_tx_queue_pump(&queue);
    assert(!queue.active && queue.count == 0u && queue.head == 0u);

    callbacks.serial_write = h9_main_uart1_tx_queue_write;
    h9_main_aprs_session_init(&session, &callbacks, &queue);
    query_length = h9_wire_encode(0x11u, gpio_query, sizeof gpio_query,
                                   query_frame, sizeof query_frame);
    assert(query_length != 0u);
    assert(h9_main_uart_ring_push_batch(&session.uart_ring, query_frame,
                                         query_length));
    assert(h9_main_aprs_session_poll(&session) == query_length);
    assert(queue.active && queue.count == 1u && capture.starts == 3u);
    assert(capture.addresses[2] == 0x10000u && capture.lengths[2] == 11u);
    assert(storage[0][0] == 0xC0u && storage[0][10] == 0xC0u);
}

int main(void)
{
    assert(h9_aprs_clock_cfgr_value(0xABCDFFFFu, 0xE123u) == 0xABCD2123u);
    test_wire_vectors();
    test_aprs_systick();
    test_wire_stream_maximum();
    test_main_aprs_wire_envelope();
    test_main_aprs_wire_receiver();
    test_main_ax25_packet();
    test_main_received_aprs_packet();
    test_main_gps_sentence_frames();
    test_main_gpio_query_reply();
    test_main_control_frames();
    test_main_uart_ring();
    test_main_aprs_session();
    test_main_policy();
    test_main_tx_service();
    test_main_boot_record();
    test_main_serial_profile();
    test_main_serial_dma_read();
    test_main_uart1_irq();
    test_main_uart1_tx();
    test_main_uart1_tx_queue();
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
    test_aprs_tx_runtime();
    test_aprs_controller();
    test_aprs_uart_receive_batches();
    test_aprs_uart1_dma_interrupts();
    test_aprs_uart1_board_setup();
    test_aprs_uart2_board_setup_and_raw_write();
    test_aprs_uart2_irq_forward();
    test_aprs_gpio_board();
    test_aprs_wave_output();
    test_aprs_adc_board();
    test_aprs_sample_timer();
    test_aprs_adc_sample_irq();
    test_aprs_internal_clock_selection();
    test_aprs_uart1_reply_transmit();
    return 0;
}
