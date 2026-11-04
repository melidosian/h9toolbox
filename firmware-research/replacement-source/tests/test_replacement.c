#include "h9_aprs_controller.h"
#include "h9_aprs_adc_board.h"
#include "h9_aprs_demod.h"
#include "h9_aprs_frame.h"
#include "h9_aprs_gpio_board.h"
#include "h9_aprs_bitstream.h"
#include "h9_aprs_boot_banner.h"
#include "h9_aprs_clock.h"
#include "h9_aprs_ring.h"
#include "h9_aprs_runtime_status.h"
#include "h9_aprs_sample_timer.h"
#include "h9_aprs_status.h"
#include "h9_aprs_systick.h"
#include "h9_aprs_tx.h"
#include "h9_aprs_tx_defaults.h"
#include "h9_aprs_tx_gate.h"
#include "h9_aprs_tx_packet.h"
#include "h9_aprs_tx_service.h"
#include "h9_aprs_tx_runtime.h"
#include "h9_aprs_uart_receive.h"
#include "h9_aprs_uart1_dma.h"
#include "h9_aprs_uart1_board.h"
#include "h9_aprs_uart1_tx.h"
#include "h9_aprs_uart2_board.h"
#include "h9_aprs_uart2_maintenance.h"
#include "h9_aprs_uart2_dma.h"
#include "h9_aprs_uart2_rx.h"
#include "h9_aprs_oscillator.h"
#include "h9_aprs_wave_output.h"
#include "h9_main_aprs.h"
#include "h9_main_aprs_control.h"
#include "h9_main_aprs_metadata.h"
#include "h9_main_aprs_mute.h"
#include "h9_main_aprs_receive.h"
#include "h9_main_aprs_session.h"
#include "h9_main_aprs_uart1_link.h"
#include "h9_main_aprs_wire.h"
#include "h9_main_ax25.h"
#include "h9_main_beacon.h"
#include "h9_main_gps_fix.h"
#include "h9_main_audio_arbiter.h"
#include "h9_main_rf_output.h"
#include "h9_main_rf_state.h"
#include "h9_main_speaker_gate.h"
#include "h9_main_rf_registers.h"
#include "h9_main_boot_record.h"
#include "h9_main_boot_validator.h"
#include "h9_main_board_markers.h"
#include "h9_main_clock_probe.h"
#include "h9_main_clock_handshake.h"
#include "h9_main_clock_apply.h"
#include "h9_main_clock_rom_mode.h"
#include "h9_main_clock_timing.h"
#include "h9_main_core_list.h"
#include "h9_main_heap_bitmap.h"
#include "h9_main_exception.h"
#include "h9_main_irq_registration.h"
#include "h9_main_memory_window.h"
#include "h9_main_post_clock_rom.h"
#include "h9_main_post_map_gate.h"
#include "h9_main_task_registry.h"
#include "h9_main_timer3_irq.h"
#include "h9_main_serial_profile.h"
#include "h9_main_serial_bus.h"
#include "h9_main_serial_commands.h"
#include "h9_main_serial_gate.h"
#include "h9_main_serial_port.h"
#include "h9_main_serial_dma.h"
#include "h9_main_uart1_irq.h"
#include "h9_main_uart1_board.h"
#include "h9_main_uart1_baud.h"
#include "h9_main_uart1_tx.h"
#include "h9_main_uart1_tx_queue.h"
#include "h9_main_tx_service.h"
#include "h9_main_tx_frequency.h"
#include "h9_main_uart_ring.h"
#include <assert.h>
#include <string.h>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

typedef struct {
    uint8_t uart[H9_APRS_ENCODED_CAPACITY];
    size_t uart_length;
    unsigned uart_writes;
    uint8_t packet[H9_APRS_MAX_RECEIVE];
    size_t packet_length;
    uint8_t afsk_packet[H9_APRS_MAX_RECEIVE];
    size_t afsk_packet_length;
    H9AprsTxService *tx_service;
    H9AprsUart2Dma *inject_uart2_dma;
    volatile uint32_t *inject_uart2_registers;
    bool inject_uart2_once;
    H9AprsTxSettings tx_settings;
    unsigned tx_settings_changes;
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
    if (fake->inject_uart2_once) {
        static const uint8_t arriving[] = { 'B', 'C' };
        size_t index;
        fake->inject_uart2_once = false;
        assert(fake->inject_uart2_dma != NULL &&
               fake->inject_uart2_registers != NULL);
        for (index = 0; index < sizeof arriving; ++index) {
            fake->inject_uart2_registers[0] = 0x20u;
            fake->inject_uart2_registers[1] = arriving[index];
            h9_aprs_uart2_dma_usart_irq(fake->inject_uart2_dma);
            fake->inject_uart2_registers[0] = 0u;
        }
    }
}

static void queue_packet(void *opaque, const uint8_t *data, size_t length)
{
    FakeHardware *fake = (FakeHardware *)opaque;
    assert(length <= sizeof fake->packet);
    memcpy(fake->packet, data, length);
    fake->packet_length = length;
}

static void capture_tx_settings(void *opaque,
                                const H9AprsTxSettings *settings)
{
    FakeHardware *fake = (FakeHardware *)opaque;
    fake->tx_settings = *settings;
    fake->tx_settings_changes++;
    if (fake->tx_service != NULL) {
        h9_aprs_tx_service_set_timing(fake->tx_service,
                                       settings->lead_ms, settings->tail_ms);
    }
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

static size_t make_nmea(const char *body, uint8_t *output, size_t capacity)
{
    static const char digits[] = "0123456789ABCDEF";
    size_t length = strlen(body);
    size_t index;
    uint8_t checksum = 0u;
    assert(length + 6u <= capacity);
    output[0] = '$';
    for (index = 0u; index < length; ++index) {
        output[index + 1u] = (uint8_t)body[index];
        checksum ^= (uint8_t)body[index];
    }
    output[length + 1u] = '*';
    output[length + 2u] = (uint8_t)digits[checksum >> 4];
    output[length + 3u] = (uint8_t)digits[checksum & 15u];
    output[length + 4u] = '\r';
    output[length + 5u] = '\n';
    return length + 6u;
}

static void test_main_gps_fix_parser(void)
{
    H9MainGpsFix fix = { 0 };
    H9MainGpsFix previous;
    uint8_t sentence[160];
    size_t length;
    length = make_nmea(
        "GNRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,231126,,,A",
        sentence, sizeof sentence);
    assert(h9_main_gps_parse_fix(sentence, length, &fix) ==
           H9_MAIN_GPS_RMC);
    assert(fix.has_position && fix.has_time && fix.has_date);
    assert(fix.latitude_e7 == 481173000 &&
           fix.longitude_e7 == 115166667);
    assert(fix.hour == 12u && fix.minute == 35u && fix.second == 19u);
    assert(fix.day == 23u && fix.month == 11u && fix.year == 2026u);
    previous = fix;
    sentence[length - 4u] ^= 1u;
    assert(h9_main_gps_parse_fix(sentence, length, &fix) ==
           H9_MAIN_GPS_UNRECOGNIZED);
    assert(memcmp(&fix, &previous, sizeof fix) == 0);
    length = make_nmea(
        "GPGGA,123520,4807.038,S,01131.000,W,1,08,0.9,545.4,M,46.9,M,,",
        sentence, sizeof sentence);
    assert(h9_main_gps_parse_fix(sentence, length, &fix) ==
           H9_MAIN_GPS_GGA);
    assert(fix.has_position && fix.latitude_e7 == -481173000 &&
           fix.longitude_e7 == -115166667);
    assert(fix.second == 20u && fix.year == 2026u);
    length = make_nmea("GNRMC,123521,V,,,,,,,231126,,,N",
                       sentence, sizeof sentence);
    assert(h9_main_gps_parse_fix(sentence, length, &fix) ==
           H9_MAIN_GPS_RMC);
    assert(!fix.has_position && fix.latitude_e7 == 0 &&
           fix.longitude_e7 == 0);
    previous = fix;
    length = make_nmea(
        "GNRMC,123521,A,9100.000,N,01131.000,E,0,0,231126,,,A",
        sentence, sizeof sentence);
    assert(h9_main_gps_parse_fix(sentence, length, &fix) ==
           H9_MAIN_GPS_UNRECOGNIZED);
    assert(memcmp(&fix, &previous, sizeof fix) == 0);
}

static void test_main_rf_output_registers(void)
{
    volatile uint32_t gpio = 0xA5A50100u;
    volatile uint32_t sync = 0x12345678u;
    volatile uint32_t pwm[4] = { 0xA5A50000u, 0u, 1020u, 0u };
    H9MainRfOutput output = { &gpio, &sync, pwm };
    H9MainRfOutput target = { 0 };
    h9_main_rf_output_bind_target(&target);
    assert((uintptr_t)target.gpio_data == 0x001E5000u);
    assert((uintptr_t)target.sync_register == 0x001E0200u);
    assert((uintptr_t)target.pwm_registers == 0x001E0600u);
    assert(h9_main_rf_output_set_gpio(&output, H9_MAIN_RF_GPIO_BIT8, false));
    assert(gpio == 0xA5A50000u);
    assert(h9_main_rf_output_set_gpio(&output, H9_MAIN_RF_GPIO_BIT8, true));
    assert(gpio == 0xA5A50100u);
    assert(h9_main_rf_output_set_gpio(&output, H9_MAIN_RF_GPIO_BIT0, true));
    assert(gpio == 0xA5A50101u);
    assert(h9_main_rf_output_set_gpio(&output, H9_MAIN_RF_GPIO_BIT0, false));
    assert(gpio == 0xA5A50100u);
    assert(!h9_main_rf_output_set_gpio(&output, (H9MainRfGpioLine)2u, true));
    assert(h9_main_rf_output_set_pwm(&output, true, 64u));
    assert(pwm[3] == 256u && pwm[0] == 0xA5A50100u);
    assert(h9_main_rf_output_set_pwm(&output, false, 255u));
    assert(pwm[3] == 1020u && pwm[0] == 0xA5A50000u);
    assert(!h9_main_rf_output_set_pwm(NULL, true, 0u));
}

static void test_main_speaker_gate(void)
{
    volatile uint32_t gpio = 0xA5A50108u;
    volatile uint32_t sync = 0x12345678u;
    uint8_t state = 0xA6u;
    H9MainSpeakerGate gate = { &gpio, &sync, &state };
    H9MainSpeakerGate target = { 0 };
    h9_main_speaker_gate_bind_target(&target);
    assert((uintptr_t)target.gpio_data == 0x001E5000u);
    assert((uintptr_t)target.sync_register == 0x001E0200u);
    assert((uintptr_t)target.audio_state == 0x00011B4Du);
    assert(h9_main_speaker_gate_set(&gate, false));
    assert(gpio == 0xA5A50100u && state == 0xA6u);
    assert(h9_main_speaker_gate_set(&gate, true));
    assert(gpio == 0xA5A50108u && state == 0xA7u);
    gate.audio_state = NULL;
    assert(!h9_main_speaker_gate_set(&gate, false));
    assert(gpio == 0xA5A50108u);
}

typedef struct {
    bool effective_on;
    unsigned calls;
} FakeAudioPolicy;

static bool fake_audio_policy(void *context, bool requested_on,
                              bool *effective_on)
{
    FakeAudioPolicy *policy = (FakeAudioPolicy *)context;
    (void)requested_on;
    policy->calls++;
    *effective_on = policy->effective_on;
    return true;
}

static void test_main_audio_gate_arbiter(void)
{
    FakeAudioPolicy policy = { false, 0u };
    H9MainAudioGatePlan plan;
    assert(!h9_main_audio_gate_plan(0u, 13u, true, NULL, NULL, NULL));
    assert(h9_main_audio_gate_plan(0u, 13u, true, NULL, NULL, &plan));
    assert(plan.next_state == 4u && plan.apply_speaker && plan.speaker_on);
    assert(h9_main_audio_gate_plan(5u, 4u, false, NULL, NULL, &plan));
    assert(plan.next_state == 5u && !plan.apply_speaker);
    assert(h9_main_audio_gate_plan(5u, 13u, false, NULL, NULL, &plan));
    assert(plan.next_state == 1u && plan.apply_speaker && !plan.speaker_on);
    assert(h9_main_audio_gate_plan(1u, 13u, true, NULL, NULL, &plan));
    assert(plan.next_state == 1u && !plan.apply_speaker);

    /* Fixed stock sources bypass the broader radio-state evaluator. */
    assert(h9_main_audio_gate_plan(0u, 11u, true, NULL, NULL, &plan));
    assert(plan.next_state == 0u && plan.apply_speaker && plan.speaker_on);
    assert(!h9_main_audio_gate_plan(0u, 4u, true, NULL, NULL, &plan));
    assert(h9_main_audio_gate_plan(0u, 4u, true, fake_audio_policy,
                                   &policy, &plan));
    assert(policy.calls == 1u && !plan.apply_speaker);
    policy.effective_on = true;
    assert(h9_main_audio_gate_plan(0u, 4u, true, fake_audio_policy,
                                   &policy, &plan));
    assert(policy.calls == 2u && plan.apply_speaker && plan.speaker_on);
    assert(h9_main_audio_gate_plan(1u, 4u, false, fake_audio_policy,
                                   &policy, &plan));
    assert(policy.calls == 3u && !plan.apply_speaker);
    policy.effective_on = false;
    assert(h9_main_audio_gate_plan(1u, 4u, false, fake_audio_policy,
                                   &policy, &plan));
    assert(policy.calls == 4u && plan.apply_speaker && !plan.speaker_on);
}

typedef struct {
    uint8_t registers[2];
    uint16_t words[2];
    size_t count;
    size_t fail_at;
} FakeRfRegisterBus;

static bool fake_rf_register_write(void *opaque, uint8_t reg, uint16_t word)
{
    FakeRfRegisterBus *bus = (FakeRfRegisterBus *)opaque;
    if (bus->count == bus->fail_at || bus->count >= 2u) return false;
    bus->registers[bus->count] = reg;
    bus->words[bus->count] = word;
    ++bus->count;
    return true;
}

static void test_main_rf_register_values(void)
{
    uint16_t low;
    uint16_t high;
    uint16_t word;
    uint8_t reg;
    H9MainRfControlWords control;
    FakeRfRegisterBus bus = { { 0 }, { 0 }, 0u, 3u };
    assert(h9_main_rf_control_words(0u, true, 0xFFFFu, 0xFFFFu,
                                    &control));
    assert(control.control_word == 0xBFFFu &&
           control.mode_register == 0x35u &&
           control.mode_word == 0xF0FFu);
    assert(h9_main_rf_control_words(3u, false, 0xFFFFu, 0xFFFFu,
                                    &control));
    assert(control.control_word == 0xF7F7u &&
           control.mode_register == 0x34u &&
           control.mode_word == 0x0FFFu);
    assert(h9_main_rf_control_words(6u, true, 0xFFFFu, 0xFFFFu,
                                    &control));
    assert(control.control_word == 0xFEFFu &&
           control.mode_register == 0x34u &&
           control.mode_word == 0xFFF0u);
    assert(!h9_main_rf_control_words(7u, true, 0u, 0u, &control));
    assert(!h9_main_rf_control_words(0u, true, 0u, 0u, NULL));
    assert(h9_main_rf_frequency_words(14439000u, 0u, 120, -50,
                                      &low, &high));
    assert(((uint32_t)high << 16 | low) == 14439012u);
    assert(h9_main_rf_frequency_words(44539000u, 1u, 120, -50,
                                      &low, &high));
    assert(((uint32_t)high << 16 | low) == 44538995u);
    assert(h9_main_rf_frequency_words(14439000u, 2u, 120, -50,
                                      &low, &high));
    assert(((uint32_t)high << 16 | low) == 14439000u);
    assert(!h9_main_rf_frequency_words(1u, 0u, -50, 0,
                                       &low, &high));
    assert(h9_main_rf_write_frequency(14439000u, 0u, 120, -50,
                                      fake_rf_register_write, &bus));
    assert(bus.count == 2u && bus.registers[0] == 0x38u &&
           bus.registers[1] == 0x39u);
    assert(((uint32_t)bus.words[1] << 16 | bus.words[0]) == 14439012u);
    bus.count = 0u;
    bus.fail_at = 0u;
    assert(!h9_main_rf_write_frequency(14439000u, 0u, 0, 0,
                                       fake_rf_register_write, &bus));
    assert(bus.count == 0u);
    assert(h9_main_rf_mode_word(1u, 2u, &reg, &word) &&
           reg == 0x74u && word == 0xDF22u);
    assert(h9_main_rf_mode_word(0u, 0x1Fu, &reg, &word) &&
           reg == 0x75u && word == 0xFA02u);
    assert(h9_main_rf_mode_word(1u, 0u, &reg, &word) &&
           word == 0xF50Bu);
    assert(!h9_main_rf_mode_word(2u, 2u, &reg, &word));
    assert(h9_main_rf_af_word(0u, &word) && word == 0x6040u);
    assert(h9_main_rf_af_word(1u, &word) && word == 0x6140u);
    assert(h9_main_rf_af_word(3u, &word) && word == 0x6340u);
    assert(!h9_main_rf_af_word(0u, NULL));
    bus.count = 0u;
    bus.fail_at = 3u;
    assert(h9_main_rf_write_af(1u, fake_rf_register_write, &bus));
    assert(bus.count == 1u && bus.registers[0] == 0x47u &&
           bus.words[0] == 0x6140u);
    bus.count = 0u;
    bus.fail_at = 0u;
    assert(!h9_main_rf_write_af(1u, fake_rf_register_write, &bus));
    assert(!h9_main_rf_write_af(1u, NULL, &bus));
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
        H9_MAIN_CONTROL_07_01, H9_MAIN_CONTROL_07_03
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
    receiver.payload[0] = 0x0Au;
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

static void test_main_control_volume(void)
{
    H9MainControlVolumeState state;
    H9MainControlVolumeEffects effects;
    assert(!h9_main_control_volume_init(&state, 31u));
    assert(h9_main_control_volume_init(&state, 29u));
    assert(h9_main_control_volume_apply(&state, H9_MAIN_CONTROL_06_05,
                                         &effects));
    assert(state.level == 30u && state.held_adjustment == 1u &&
           state.timer_armed && state.volume_display_pending);
    assert(effects.level_changed && !effects.cancel_timer &&
           effects.start_timer_units == 200u);
    assert(h9_main_control_volume_apply(&state, H9_MAIN_CONTROL_06_01,
                                         &effects));
    assert(state.level == 30u && !effects.level_changed &&
           !effects.boundary_feedback && !effects.cancel_timer &&
           effects.start_timer_units == 150u);
    assert(h9_main_control_volume_apply(&state, H9_MAIN_CONTROL_06_05,
                                         &effects));
    assert(state.level == 30u && !effects.level_changed &&
           !effects.boundary_feedback && effects.cancel_timer &&
           effects.start_timer_units == 200u);
    assert(h9_main_control_volume_apply(&state, H9_MAIN_CONTROL_06_02,
                                         &effects));
    assert(state.held_adjustment == 0u && !state.timer_armed &&
           effects.cancel_timer && effects.start_timer_units == 0u);
    assert(h9_main_control_volume_apply(&state, H9_MAIN_CONTROL_06_01,
                                         &effects));
    assert(state.level == 30u && effects.boundary_feedback &&
           !effects.level_changed);
    assert(h9_main_control_volume_apply(&state, H9_MAIN_CONTROL_06_06,
                                         &effects));
    assert(state.level == 29u && state.held_adjustment == 2u &&
           effects.level_changed && effects.start_timer_units == 200u);
    assert(h9_main_control_volume_apply(&state, H9_MAIN_CONTROL_06_04,
                                         &effects));
    assert(!state.timer_armed && state.held_adjustment == 0u);
    assert(h9_main_control_volume_apply(&state, H9_MAIN_CONTROL_06_03,
                                         &effects));
    assert(state.level == 28u && effects.level_changed);
    assert(!h9_main_control_volume_apply(&state, H9_MAIN_CONTROL_07_01,
                                          &effects));
    assert(state.level == 28u);
    assert(h9_main_control_volume_init(&state, 0u));
    assert(h9_main_control_volume_apply(&state, H9_MAIN_CONTROL_06_03,
                                         &effects));
    assert(state.level == 0u && effects.boundary_feedback);
}

static void test_main_control_07_timer(void)
{
    H9MainControl07State state = { 0xC5u, 0u, 7u, 0u };
    H9MainControl07StartEffects start;
    H9MainControl07TimerEffects fired;
    assert(h9_main_control_07_03(&state, &start));
    assert(state.flags == 0x05u && state.pending == 1u &&
           state.phase == 0u && start.start_timer &&
           start.delay_units == 50u);
    state.timer_handle = 0x12345678u;
    assert(h9_main_control_07_03(&state, &start));
    assert(!start.start_timer && state.timer_handle == 0x12345678u);
    assert(h9_main_control_07_timer_fired(&state, 0u, &fired));
    assert(fired.timer_handle_low16 == 0x5678u &&
           fired.callback == H9_MAIN_CONTROL_07_CALL_6172C_WITH_2 &&
           state.timer_handle == 0u);
    state.pending = 0u;
    state.phase = 7u;
    state.timer_handle = 9u;
    assert(h9_main_control_07_03(&state, &start));
    assert(state.phase == 0u && !start.start_timer);
    assert(h9_main_control_07_timer_fired(&state, 1u, &fired));
    assert(fired.timer_handle_low16 == 9u &&
           fired.callback == H9_MAIN_CONTROL_07_CALL_ROM_0200010A);
    assert(h9_main_control_07_timer_fired(&state, 2u, &fired));
    assert(fired.callback == H9_MAIN_CONTROL_07_CALLBACK_NONE);
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
    unsigned gps_fix_count;
    unsigned control_count;
    unsigned version_count;
    unsigned serial_count;
    H9MainControlAction action;
    uint8_t source[7];
    uint8_t sentence[256];
    H9MainGpsSentenceType gps_type;
    H9MainGpsFix gps_fix;
    size_t sentence_length;
    uint8_t serial[16];
    size_t serial_length;
    uint8_t incoming[64];
    size_t incoming_length;
    size_t incoming_offset;
    size_t incoming_chunk;
    unsigned serial_read_count;
    bool invalid_read_length;
    H9MainAprsMuteLease mute_lease;
    uint32_t now_ms;
    unsigned burst_count;
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

static void session_gps_fix(void *context, H9MainGpsSentenceType type,
                            const H9MainGpsFix *fix)
{
    MainSessionCapture *capture = (MainSessionCapture *)context;
    capture->gps_fix_count++;
    capture->gps_type = type;
    capture->gps_fix = *fix;
}

static void session_control(void *context, H9MainControlAction action)
{
    MainSessionCapture *capture = (MainSessionCapture *)context;
    capture->control_count++;
    capture->action = action;
}

static void session_burst_activity(void *context, bool active)
{
    MainSessionCapture *capture = (MainSessionCapture *)context;
    capture->burst_count++;
    h9_main_aprs_mute_event(&capture->mute_lease, active, capture->now_ms);
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
    callbacks.gps_fix = session_gps_fix;
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
    assert(capture.gps_fix_count == 0u);
    {
        uint8_t nmea[160];
        size_t nmea_length = make_nmea(
            "GNRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,231126,,,A",
            nmea, sizeof nmea);
        serial_length = h9_wire_encode(0x10u, nmea, nmea_length,
                                       serial, sizeof serial);
        queue_session_bytes(&session, serial, serial_length);
        assert(h9_main_aprs_session_poll(&session) == serial_length);
        assert(capture.gps_fix_count == 1u &&
               capture.gps_type == H9_MAIN_GPS_RMC &&
               capture.gps_fix.latitude_e7 == 481173000);
    }

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
        assert(h9_main_aprs_session_take_ready(&session));
        assert(!h9_main_aprs_session_take_ready(&session));
        assert(!h9_main_aprs_session_take_ready(NULL));
        capture.incoming_offset = 0u;
        memcpy(capture.incoming, py_ready, sizeof py_ready);
        assert(h9_main_aprs_session_poll(&session) == 0u);
        assert(session.py_ready);
        capture.incoming_offset = 0u;
        memcpy(capture.incoming, py_query, sizeof py_query);
        assert(h9_main_aprs_session_poll(&session) == 0u);
        assert(capture.serial_count == 2u &&
               capture.serial_length == sizeof thd_reply &&
               memcmp(capture.serial, thd_reply, sizeof thd_reply) == 0);
        assert(h9_main_aprs_session_take_ready(&session));
    }
}

static void test_aprs_burst_activity_link(void)
{
    FakeHardware fake = { 0 };
    H9AprsHardware hardware = { 0 };
    H9AprsController controller;
    H9MainAprsSession session;
    H9MainAprsSessionCallbacks callbacks = { 0 };
    MainSessionCapture capture = { 0 };
    H9MainAprsConfig config = { true, true, H9_RX_B, H9_TX_B_LOCK };
    H9AudioPlan audio;
    uint8_t malformed[16];
    const uint8_t bad_version[] = { 2u, 1u };
    size_t malformed_length;

    hardware.context = &fake;
    hardware.uart_write = uart_write;
    h9_aprs_controller_init(&controller, &hardware);
    callbacks.burst_activity = session_burst_activity;
    h9_main_aprs_session_init(&session, &callbacks, &capture);
    assert(!h9_aprs_controller_report_burst_activity(&controller, 0u));
    atomic_store_explicit(&controller.demod.gate_flags, 1u,
                          memory_order_release);
    capture.now_ms = 100u;
    assert(h9_aprs_controller_report_burst_activity(&controller, 100u));
    assert(fake.uart_length == 5u && fake.uart[0] == 0xC0u &&
           fake.uart[1] == 0x52u && fake.uart[2] == 1u &&
           fake.uart[3] == 1u && fake.uart[4] == 0xC0u);
    queue_session_bytes(&session, fake.uart, fake.uart_length);
    assert(h9_main_aprs_session_poll(&session) == fake.uart_length);
    assert(capture.burst_count == 1u);
    audio = h9_main_aprs_mute_plan(&config, &capture.mute_lease,
                                   H9_SIDE_B, true, 100u);
    assert(audio.decoder_feed && !audio.speaker_audio &&
           !audio.bluetooth_audio);
    audio = h9_main_aprs_mute_plan(&config, &capture.mute_lease,
                                   H9_SIDE_A, true, 100u);
    assert(!audio.decoder_feed && audio.speaker_audio &&
           audio.bluetooth_audio);
    audio = h9_main_aprs_mute_plan(&config, &capture.mute_lease,
                                   H9_SIDE_B, false, 100u);
    assert(audio.decoder_feed && audio.speaker_audio &&
           audio.bluetooth_audio);
    assert(h9_main_aprs_mute_should_silence(&capture.mute_lease,
                                            true, true, 100u));
    assert(!h9_main_aprs_mute_should_silence(&capture.mute_lease,
                                             false, true, 100u));
    assert(!h9_main_aprs_mute_should_silence(&capture.mute_lease,
                                             true, false, 100u));
    fake.uart_length = 0u;
    assert(!h9_aprs_controller_report_burst_activity(&controller, 349u));
    assert(h9_aprs_controller_report_burst_activity(&controller, 350u));
    capture.now_ms = 350u;
    queue_session_bytes(&session, fake.uart, fake.uart_length);
    assert(h9_main_aprs_session_poll(&session) == fake.uart_length);
    assert(capture.burst_count == 2u);
    assert(h9_main_aprs_mute_should_silence(&capture.mute_lease,
                                            true, true, 1099u));
    assert(!h9_main_aprs_mute_should_silence(&capture.mute_lease,
                                             true, true, 1100u));
    audio = h9_main_aprs_mute_plan(&config, &capture.mute_lease,
                                   H9_SIDE_B, true, 1100u);
    assert(audio.decoder_feed && audio.speaker_audio &&
           audio.bluetooth_audio);
    fake.uart_length = 0u;
    atomic_store_explicit(&controller.demod.gate_flags, 0u,
                          memory_order_release);
    assert(h9_aprs_controller_report_burst_activity(&controller, 351u));
    capture.now_ms = 351u;
    queue_session_bytes(&session, fake.uart, fake.uart_length);
    assert(h9_main_aprs_session_poll(&session) == fake.uart_length);
    assert(capture.burst_count == 3u &&
           !h9_main_aprs_mute_should_silence(&capture.mute_lease,
                                              true, true, 351u));
    malformed_length = h9_wire_encode(0x52u, bad_version,
                                      sizeof bad_version, malformed,
                                      sizeof malformed);
    assert(malformed_length != 0u);
    queue_session_bytes(&session, malformed, malformed_length);
    assert(h9_main_aprs_session_poll(&session) == malformed_length);
    assert(capture.burst_count == 3u);
}

static void test_main_rf_live_route(void)
{
    volatile uint8_t dual_watch = 1u;
    volatile uint8_t route_side = 0u;
    volatile uint8_t side_state = 0x80u;
    volatile uint32_t working_rx = 44539000u;
    volatile uint32_t working_tx = 44539000u;
    volatile uint32_t a_rx = 44539000u;
    volatile uint32_t a_tx = 44539000u;
    volatile uint32_t b_rx = 14439000u;
    volatile uint32_t b_tx = 14439000u;
    H9MainRfStateMemory target = { 0 };
    H9MainRfStateMemory memory = {
        &dual_watch, &route_side, &side_state,
        &working_rx, &working_tx, &a_rx, &a_tx, &b_rx, &b_tx
    };
    H9MainRfSnapshot snapshot;
    H9MainRfRouteSaved saved = { 0 };
    H9MainAprsConfig config = { true, false, H9_RX_B, H9_TX_B_LOCK };
    H9TxPlan plan = h9_main_tx_plan(&config, H9_SIDE_B,
                                    445390000u, 144390000u);
    h9_main_rf_state_bind_target(&target);
    assert((uintptr_t)target.dual_watch == 0x12BA7u &&
           (uintptr_t)target.route_side == 0x11AECu &&
           (uintptr_t)target.channel_side_state == 0x11B50u &&
           (uintptr_t)target.working_rx_10hz == 0x11C8Cu &&
           (uintptr_t)target.working_tx_10hz == 0x11C9Cu &&
           (uintptr_t)target.a_rx_10hz == 0x11CA0u &&
           (uintptr_t)target.a_tx_10hz == 0x11CA4u &&
           (uintptr_t)target.b_rx_10hz == 0x11CA8u &&
           (uintptr_t)target.b_tx_10hz == 0x11CACu);
    assert(plan.allowed && plan.side == H9_SIDE_B);
    assert(h9_main_rf_state_read(&memory, &snapshot));
    assert(snapshot.logical_side == H9_SIDE_B &&
           snapshot.setup_side == H9_SIDE_A);
    assert(!h9_main_rf_snapshot_matches_plan(&snapshot, &config, &plan));
    assert(h9_main_rf_route_prepare(&memory, &plan, &saved));
    assert(saved.active && saved.route_side == 0u &&
           route_side == 1u && side_state == 0x80u);
    assert(!h9_main_rf_route_prepare(&memory, &plan, &saved));
    working_rx = b_rx;
    working_tx = b_tx;
    assert(h9_main_rf_state_read(&memory, &snapshot));
    assert(h9_main_rf_snapshot_matches_plan(&snapshot, &config, &plan));
    assert(h9_main_rf_route_restore(&memory, &saved));
    assert(route_side == 0u && side_state == 0x80u && !saved.active);
    assert(!h9_main_rf_route_restore(&memory, &saved));
    route_side = 1u;
    working_rx = b_rx;
    working_tx = b_tx;
    assert(h9_main_rf_state_read(&memory, &snapshot));
    assert(h9_main_rf_snapshot_matches_plan(&snapshot, &config, &plan));
    snapshot.b.transmit_10hz = 14500000u;
    snapshot.working.transmit_10hz = 14500000u;
    assert(!h9_main_rf_snapshot_matches_plan(&snapshot, &config, &plan));
    snapshot.b.transmit_10hz = b_tx;
    snapshot.working.transmit_10hz = b_tx;
    plan.expected_tx_hz++;
    assert(!h9_main_rf_snapshot_matches_plan(&snapshot, &config, &plan));
    plan.expected_tx_hz--;
    working_tx = a_tx;
    assert(h9_main_rf_state_read(&memory, &snapshot));
    assert(!h9_main_rf_snapshot_matches_plan(&snapshot, &config, &plan));
    working_tx = b_tx;
    dual_watch = 0u;
    route_side = 0u;
    assert(h9_main_rf_route_prepare(&memory, &plan, &saved));
    assert(route_side == 0u && side_state == 0x80u);
    assert(h9_main_rf_route_restore(&memory, &saved));
    assert(h9_main_rf_state_read(&memory, &snapshot));
    assert(h9_main_rf_snapshot_matches_plan(&snapshot, &config, &plan));
    dual_watch = 1u;
    route_side = 1u;
    side_state = 0u;
    assert(h9_main_rf_state_read(&memory, &snapshot));
    assert(!h9_main_rf_snapshot_matches_plan(&snapshot, &config, &plan));
    config.tx_choice = H9_TX_B_AUTO;
    plan = h9_main_tx_plan(&config, H9_SIDE_A,
                            445390000u, 144390000u);
    assert(plan.allowed && plan.restore_side_after_send);
    assert(h9_main_rf_snapshot_matches_plan(&snapshot, &config, &plan));
    assert(h9_main_rf_route_prepare(&memory, &plan, &saved));
    assert(side_state == 0x80u && route_side == 1u);
    assert(h9_main_rf_route_restore(&memory, &saved));
    assert(side_state == 0u);
    assert(!h9_main_rf_route_prepare(&memory,
        &(H9TxPlan){ true, H9_SIDE_B, 144390000u, false }, &saved));
    assert(side_state == 0u && route_side == 1u);
    config.tx_choice = H9_TX_A_AUTO;
    assert(!h9_main_rf_snapshot_matches_plan(&snapshot, &config, &plan));
    config.tx_choice = H9_TX_B_AUTO;
    config.aprs_enabled = false;
    assert(!h9_main_rf_snapshot_matches_plan(&snapshot, &config, &plan));
    assert(!h9_main_rf_state_read(NULL, &snapshot));
    memory.b_tx_10hz = NULL;
    assert(!h9_main_rf_state_read(&memory, &snapshot));
}

static void test_main_policy(void)
{
    H9MainAprsConfig cfg = { true, true, H9_RX_B, H9_TX_B_LOCK };
    const H9MainRfChannelFrequencies a = { 44539000u, 44540000u };
    const H9MainRfChannelFrequencies b = { 14439000u, 14441000u };
    const H9MainRfChannelFrequencies empty_b = { 0u, 0u };
    const H9MainRfTxOverride tx_override = { 14500000u, 14510000u, 2u };
    H9MainRfChannelFrequencies working = { 0u, 0u };
    H9Side selected_side = H9_SIDE_A;
    uint32_t candidate_10hz = 0u;
    uint8_t shift_mode = 0u;
    H9AudioPlan audio;
    H9TxPlan tx;
    uint8_t side_state = 0x2Du;
    assert(h9_main_side_from_state(side_state) == H9_SIDE_A);
    side_state = h9_main_track_side_bit(side_state, true, 144390000u,
                                         445390000u, 144390000u);
    assert(side_state == 0xADu);
    assert(h9_main_side_from_state(side_state) == H9_SIDE_B);
    assert(h9_main_rf_setup_side(false, 0u, side_state) == H9_SIDE_B);
    assert(h9_main_rf_setup_side(false, 1u, 0x2Du) == H9_SIDE_A);
    assert(h9_main_rf_setup_side(true, 0u, side_state) == H9_SIDE_A);
    assert(h9_main_rf_setup_side(true, 1u, 0x2Du) == H9_SIDE_B);
    assert(h9_main_rf_setup_side(true, 0xFFu, 0x2Du) == H9_SIDE_B);
    assert(h9_main_rf_select_working_frequencies(true, 1u, 0x2Du,
                                                  &a, &b, &working,
                                                  &selected_side));
    assert(selected_side == H9_SIDE_B &&
           working.receive_10hz == b.receive_10hz &&
           working.transmit_10hz == b.transmit_10hz);
    assert(h9_main_rf_tx_candidate_10hz(&working, false, &candidate_10hz));
    assert(candidate_10hz == b.transmit_10hz);
    assert(h9_main_rf_tx_candidate_10hz(&working, true, &candidate_10hz));
    assert(candidate_10hz == b.receive_10hz);
    assert(h9_main_rf_tx_input(&working, 1u, NULL, false,
                               &candidate_10hz, &shift_mode));
    assert(candidate_10hz == b.transmit_10hz && shift_mode == 1u);
    assert(h9_main_rf_tx_input(NULL, 0u, &tx_override, false,
                               &candidate_10hz, &shift_mode));
    assert(candidate_10hz == tx_override.first_10hz && shift_mode == 2u);
    assert(h9_main_rf_tx_input(&working, 1u, &tx_override, true,
                               &candidate_10hz, &shift_mode));
    assert(candidate_10hz == tx_override.second_10hz && shift_mode == 2u);
    assert(h9_main_rf_select_working_frequencies(false, 1u, 0x2Du,
                                                  &a, &b, &working,
                                                  &selected_side));
    assert(selected_side == H9_SIDE_A &&
           working.receive_10hz == a.receive_10hz &&
           working.transmit_10hz == a.transmit_10hz);
    assert(!h9_main_rf_select_working_frequencies(true, 1u, 0x2Du,
                                                   &a, &empty_b, &working,
                                                   &selected_side));
    assert(selected_side == H9_SIDE_A &&
           working.transmit_10hz == a.transmit_10hz);
    assert(!h9_main_rf_tx_candidate_10hz(NULL, false, &candidate_10hz));
    assert(h9_main_track_side_bit(side_state, false, 445390000u,
                                  445390000u, 144390000u) == side_state);
    assert(h9_main_track_side_bit(side_state, true, 145000000u,
                                  445390000u, 144390000u) == side_state);
    assert(h9_main_track_side_bit(side_state, true, 445390000u,
                                  445390000u, 144390000u) == 0x2Du);
    /* The stock branch tests A first when both channel values match. */
    assert(h9_main_track_side_bit(side_state, true, 144390000u,
                                  144390000u, 144390000u) == 0x2Du);
    audio = h9_main_audio_plan(&cfg, H9_SIDE_B, false);
    assert(audio.decoder_feed && audio.speaker_audio && audio.bluetooth_audio);
    audio = h9_main_audio_plan(&cfg, H9_SIDE_B, true);
    assert(audio.decoder_feed && !audio.speaker_audio && !audio.bluetooth_audio);
    audio = h9_main_audio_plan(&cfg, H9_SIDE_A, true);
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

static void test_main_tx_frequency_precheck(void)
{
    H9MainTxFrequencyPolicy policy = { 0 };
    H9MainTxLimits limits = h9_main_stock_tx_limits;
    uint32_t tx;
    assert(h9_main_tx_apply_shift(14439000u, false, 1u, 60000u, &tx));
    assert(tx == 14379000u);
    assert(h9_main_tx_apply_shift(14439000u, false, 2u, 60000u, &tx));
    assert(tx == 14499000u);
    assert(h9_main_tx_apply_shift(14439000u, true, 2u, 60000u, &tx));
    assert(tx == 14439000u);
    assert(!h9_main_tx_apply_shift(1u, false, 1u, 60000u, &tx));
    assert(!h9_main_tx_apply_shift(UINT32_MAX, false, 2u, 1u, &tx));
    assert(!h9_main_tx_apply_shift(14439000u, false, 3u, 0u, &tx));
    assert(h9_main_tx_hz_allowed(&policy, 144390000u));
    assert(!h9_main_tx_hz_allowed(&policy, 144390001u));
    assert(!h9_main_tx_frequency_allowed(&policy, 13599999u));
    assert(!h9_main_tx_frequency_allowed(&policy, 20000000u));
    policy.allow_200_to_300_mhz = true;
    assert(h9_main_tx_frequency_allowed(&policy, 20000000u));
    assert(!h9_main_tx_frequency_allowed(&policy, 30000000u));
    policy.allow_300_to_400_mhz = true;
    assert(h9_main_tx_frequency_allowed(&policy, 30000000u));
    assert(!h9_main_tx_frequency_allowed(&policy, 50000000u));
    policy.allow_500_mhz_plus = true;
    assert(h9_main_tx_frequency_allowed(&policy, 50000000u));
    policy.region_mode = 1u;
    assert(h9_main_tx_frequency_allowed(&policy, 14400000u));
    assert(h9_main_tx_frequency_allowed(&policy, 14800000u));
    assert(!h9_main_tx_frequency_allowed(&policy, 14800001u));
    assert(!h9_main_tx_frequency_allowed(&policy, 41999999u));
    assert(h9_main_tx_frequency_allowed(&policy, 42000000u));
    assert(h9_main_tx_frequency_allowed(&policy, 45000000u));
    assert(!h9_main_tx_frequency_allowed(&policy, 45000001u));
    policy.region_mode = 2u;
    assert(h9_main_tx_frequency_allowed(&policy, 46256250u));
    assert(h9_main_tx_frequency_allowed(&policy, 46772500u));
    assert(!h9_main_tx_frequency_allowed(&policy, 46256251u));
    policy.region_mode = 3u;
    assert(!h9_main_tx_frequency_allowed(&policy, 14439000u));
    policy.region_mode = 0u;
    tx = 14439000u;
    assert(h9_main_tx_limit_check(&tx, &limits, 0u, false, false));
    assert(tx == 14439000u);
    tx = 13599999u;
    assert(!h9_main_tx_limit_check(&tx, &limits, 0u, false, false));
    assert(tx == 17400000u);
    tx = 17400001u;
    assert(!h9_main_tx_limit_check(&tx, &limits, 0u, false, false));
    assert(tx == 13600000u);
    tx = 39000u;
    assert(h9_main_tx_limit_check(&tx, &limits, 0u, true, false));
    tx = 52000001u;
    assert(!h9_main_tx_limit_check(&tx, &limits, 1u, false, false));
    assert(tx == 40000000u);
    tx = 25000000u;
    assert(h9_main_tx_limit_check(&tx, &limits, 0u, false, true));
    assert(!h9_main_tx_limit_check(&tx, &limits, 0u, false, false));
    assert(tx == 13600000u);
    assert(h9_main_tx_hz_allowed_with_limits(&policy, &limits,
                                              445390000u));
    assert(!h9_main_tx_hz_allowed_with_limits(&policy, &limits,
                                               185000000u));
    assert(h9_main_tx_hz_allowed_with_limits(&policy, &limits,
                                              144390000u));
}

typedef struct {
    H9Side current_side;
    H9Side prepared_side;
    uint32_t prepared_hz;
    bool prepared_restore_side;
    volatile uint8_t dual_watch;
    volatile uint8_t route_side;
    volatile uint8_t side_state;
    H9MainRfRouteSaved saved_route;
    bool fail_prepare;
    bool fail_send;
    bool fail_stop;
    bool wrong_readback_side;
    bool wrong_readback_hz;
    bool wrong_route_side;
    bool wrong_working_pair;
    bool changed_selected_frequency;
    bool changed_logical_side;
    bool route_reserved;
    bool fail_begin_route;
    bool fail_end_route;
    H9MainTxFrequencyPolicy frequency_policy;
    unsigned begin_calls;
    unsigned end_calls;
    unsigned validate_calls;
    unsigned prepare_calls;
    unsigned readback_calls;
    unsigned snapshot_calls;
    unsigned send_calls;
    unsigned stop_calls;
    unsigned restore_calls;
    unsigned step;
} FakeMainTx;

static bool main_begin_route(void *opaque)
{
    FakeMainTx *fake = (FakeMainTx *)opaque;
    assert(!fake->route_reserved);
    fake->begin_calls++;
    if (fake->fail_begin_route) return false;
    fake->route_reserved = true;
    return true;
}

static bool main_end_route(void *opaque)
{
    FakeMainTx *fake = (FakeMainTx *)opaque;
    assert(fake->route_reserved);
    fake->route_reserved = false;
    fake->end_calls++;
    return !fake->fail_end_route;
}

static bool main_read_side(void *opaque, H9Side *side)
{
    FakeMainTx *fake = (FakeMainTx *)opaque;
    assert(fake->route_reserved);
    *side = fake->current_side;
    return true;
}

static bool main_validate_frequency(void *opaque, uint32_t hz)
{
    FakeMainTx *fake = (FakeMainTx *)opaque;
    assert(fake->route_reserved);
    assert(fake->step == 0u);
    ++fake->validate_calls;
    return h9_main_tx_hz_allowed_with_limits(&fake->frequency_policy,
                                              &h9_main_stock_tx_limits, hz);
}

static bool main_prepare(void *opaque, const H9TxPlan *plan)
{
    FakeMainTx *fake = (FakeMainTx *)opaque;
    H9MainRfStateMemory memory = { 0 };
    assert(fake->route_reserved);
    assert(fake->step == 0u);
    assert(fake->validate_calls == 1u);
    assert(plan != NULL && plan->allowed);
    fake->dual_watch = 1u;
    fake->side_state = fake->current_side == H9_SIDE_B ? 0x80u : 0u;
    memory.dual_watch = &fake->dual_watch;
    memory.route_side = &fake->route_side;
    memory.channel_side_state = &fake->side_state;
    assert(h9_main_rf_route_prepare(&memory, plan, &fake->saved_route));
    fake->step = 1u;
    fake->prepare_calls++;
    fake->prepared_side = plan->side;
    fake->prepared_hz = plan->expected_tx_hz;
    fake->prepared_restore_side = plan->restore_side_after_send;
    return !fake->fail_prepare;
}

static bool main_read_prepared(void *opaque, H9Side *side, uint32_t *hz)
{
    FakeMainTx *fake = (FakeMainTx *)opaque;
    assert(fake->route_reserved);
    assert(fake->step == 1u);
    fake->step = 2u;
    fake->readback_calls++;
    *side = fake->wrong_readback_side ? H9_SIDE_A : fake->prepared_side;
    *hz = fake->wrong_readback_hz ? 445390000u : fake->prepared_hz;
    return true;
}

static bool main_read_rf_snapshot(void *opaque, H9MainRfSnapshot *snapshot)
{
    FakeMainTx *fake = (FakeMainTx *)opaque;
    assert(fake->route_reserved);
    assert(fake->step == 2u);
    fake->snapshot_calls++;
    snapshot->dual_watch = fake->dual_watch != 0u;
    snapshot->logical_side = fake->changed_logical_side ?
                             H9_SIDE_A : h9_main_side_from_state(fake->side_state);
    snapshot->setup_side = fake->wrong_route_side ?
        H9_SIDE_A : h9_main_rf_setup_side(snapshot->dual_watch,
                                          fake->route_side, fake->side_state);
    snapshot->a.receive_10hz = 44539000u;
    snapshot->a.transmit_10hz = 44539000u;
    snapshot->b.receive_10hz = 14439000u;
    snapshot->b.transmit_10hz = 14439000u;
    snapshot->working = fake->wrong_working_pair ? snapshot->a :
                        (fake->prepared_side == H9_SIDE_B ?
                         snapshot->b : snapshot->a);
    if (fake->changed_selected_frequency) {
        snapshot->b.transmit_10hz = 14500000u;
        snapshot->working.transmit_10hz = 14500000u;
    }
    return true;
}

static bool main_send_packet(void *opaque, const uint8_t *packet, size_t length)
{
    FakeMainTx *fake = (FakeMainTx *)opaque;
    assert(fake->route_reserved);
    assert(fake->step == 2u);
    assert(packet[0] == 0x82u && length == 1u);
    fake->step = 3u;
    fake->send_calls++;
    return !fake->fail_send;
}

static bool main_stop_tx(void *opaque)
{
    FakeMainTx *fake = (FakeMainTx *)opaque;
    assert(fake->route_reserved);
    assert(fake->step == 1u || fake->step == 2u || fake->step == 3u);
    fake->step = 4u;
    fake->stop_calls++;
    return !fake->fail_stop;
}

static bool main_restore_receive(void *opaque, H9Side previous_side)
{
    FakeMainTx *fake = (FakeMainTx *)opaque;
    H9MainRfStateMemory memory = { 0 };
    assert(fake->route_reserved);
    assert(fake->step == 4u);
    assert(previous_side == fake->current_side);
    memory.dual_watch = &fake->dual_watch;
    memory.route_side = &fake->route_side;
    memory.channel_side_state = &fake->side_state;
    assert(h9_main_rf_route_restore(&memory, &fake->saved_route));
    fake->step = 5u;
    fake->restore_calls++;
    return true;
}

static void test_main_tx_service(void)
{
    const uint8_t packet[] = { 0x82u };
    const H9MainTxHardware hardware = {
        main_begin_route, main_end_route,
        main_read_side, main_validate_frequency, main_prepare, main_read_prepared,
        main_read_rf_snapshot, main_send_packet, main_stop_tx,
        main_restore_receive
    };
    H9MainAprsConfig cfg = { true, false, H9_RX_B, H9_TX_B_LOCK };
    FakeMainTx fake = { 0 };
    H9MainTxResult result;

    fake.current_side = H9_SIDE_A;
    result = h9_main_tx_send(&cfg, 445390000u, 144390000u,
                             packet, sizeof packet, &hardware, &fake);
    assert(result == H9_MAIN_TX_REJECTED && fake.prepare_calls == 0u &&
           fake.begin_calls == 1u && fake.end_calls == 1u &&
           !fake.route_reserved);

    memset(&fake, 0, sizeof fake);
    fake.fail_begin_route = true;
    result = h9_main_tx_send(&cfg, 445390000u, 144390000u,
                             packet, sizeof packet, &hardware, &fake);
    assert(result == H9_MAIN_TX_ROUTE_BUSY && fake.begin_calls == 1u &&
           fake.end_calls == 0u && fake.prepare_calls == 0u);

    memset(&fake, 0, sizeof fake);
    fake.current_side = H9_SIDE_B;
    fake.frequency_policy.region_mode = 1u;
    result = h9_main_tx_send(&cfg, 445390000u, 149000000u,
                             packet, sizeof packet, &hardware, &fake);
    assert(result == H9_MAIN_TX_REJECTED && fake.validate_calls == 1u &&
           fake.prepare_calls == 0u && fake.send_calls == 0u);

    memset(&fake, 0, sizeof fake);
    fake.current_side = H9_SIDE_B;
    result = h9_main_tx_send(&cfg, 445390000u, 144390000u,
                             packet, sizeof packet, &hardware, &fake);
    assert(result == H9_MAIN_TX_SENT && fake.step == 5u);
    assert(fake.begin_calls == 1u && fake.end_calls == 1u &&
           !fake.route_reserved);
    assert(fake.prepared_side == H9_SIDE_B && fake.prepared_hz == 144390000u);
    assert(!fake.prepared_restore_side);
    assert(fake.route_side == 0u && fake.side_state == 0x80u);
    assert(fake.send_calls == 1u && fake.stop_calls == 1u &&
           fake.restore_calls == 1u && fake.snapshot_calls == 1u);

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
    fake.wrong_route_side = true;
    result = h9_main_tx_send(&cfg, 445390000u, 144390000u,
                             packet, sizeof packet, &hardware, &fake);
    assert(result == H9_MAIN_TX_ROUTE_FAILED && fake.send_calls == 0u);
    assert(fake.snapshot_calls == 1u && fake.stop_calls == 1u &&
           fake.restore_calls == 1u);

    memset(&fake, 0, sizeof fake);
    fake.current_side = H9_SIDE_B;
    fake.wrong_working_pair = true;
    result = h9_main_tx_send(&cfg, 445390000u, 144390000u,
                             packet, sizeof packet, &hardware, &fake);
    assert(result == H9_MAIN_TX_ROUTE_FAILED && fake.send_calls == 0u);

    memset(&fake, 0, sizeof fake);
    fake.current_side = H9_SIDE_B;
    fake.changed_selected_frequency = true;
    result = h9_main_tx_send(&cfg, 445390000u, 144390000u,
                             packet, sizeof packet, &hardware, &fake);
    assert(result == H9_MAIN_TX_ROUTE_FAILED && fake.send_calls == 0u);
    assert(fake.stop_calls == 1u && fake.restore_calls == 1u);

    memset(&fake, 0, sizeof fake);
    fake.current_side = H9_SIDE_B;
    fake.changed_logical_side = true;
    result = h9_main_tx_send(&cfg, 445390000u, 144390000u,
                             packet, sizeof packet, &hardware, &fake);
    assert(result == H9_MAIN_TX_ROUTE_FAILED && fake.send_calls == 0u);

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
    assert(fake.prepared_restore_side);
    assert(fake.restore_calls == 1u && fake.current_side == H9_SIDE_A);
    assert(fake.route_side == 0u && fake.side_state == 0u);

    memset(&fake, 0, sizeof fake);
    fake.current_side = H9_SIDE_B;
    fake.fail_end_route = true;
    result = h9_main_tx_send(&cfg, 445390000u, 144390000u,
                             packet, sizeof packet, &hardware, &fake);
    assert(result == H9_MAIN_TX_CLEANUP_FAILED &&
           fake.send_calls == 1u && fake.stop_calls == 1u &&
           fake.restore_calls == 1u && fake.end_calls == 1u &&
           !fake.route_reserved);
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
    assert(controller.tx.lead_ms == H9_APRS_STOCK_TX_LEAD_MS &&
           controller.tx.tail_ms == H9_APRS_STOCK_TX_TAIL_MS);
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
           H9_APRS_TX_STARTING);
    while (samples < 20000 && h9_aprs_tx_service_next_sample(&service, &sample)) {
        (void)h9_aprs_demod_sample(&demodulator, (int16_t)sample - 128);
        while (h9_aprs_demod_pop_byte(&demodulator, &receive_byte))
            (void)h9_aprs_frame_feed(&receive_frame, receive_byte,
                                     emit_packet, &decoded);
        samples++;
    }
    assert(samples >= 5376 && samples < 20000);
    assert(service.phase == H9_APRS_TX_IDLE);
    assert(h9_aprs_tx_service_take_encoded_event(&service));
    assert(!h9_aprs_tx_service_take_encoded_event(&service));
    assert(decoded.packets == 1 && decoded.length == packet_length);
    assert(memcmp(decoded.bytes, expected, packet_length) == 0);
    assert(h9_aprs_tx_service_submit(&service, packet, packet_length));
    assert(h9_aprs_tx_service_step(&service, true, true, true, 255) ==
           H9_APRS_TX_IDLE);
    assert(!h9_aprs_tx_service_next_sample(&service, &sample));
    assert(!h9_aprs_tx_service_take_encoded_event(&service));
}

static void test_aprs_tx_interrupt_handoff(void)
{
    H9AprsTxService service;
    const uint8_t packet[20] = { 0x82u, 0xA0u, 0xA4u, 0xA6u };
    uint8_t sample;
    unsigned attempt;
    h9_aprs_tx_service_init(&service, 0u, 0u, 0u);
    assert(h9_aprs_tx_service_submit(&service, packet, sizeof packet));
    for (attempt = 0; attempt < 8u &&
         service.phase == H9_APRS_TX_WAITING; ++attempt)
        (void)h9_aprs_tx_service_step(&service, false, false, false, 255u);
    assert(service.phase == H9_APRS_TX_STARTING);
    assert(h9_aprs_tx_service_cancel(&service) == H9_APRS_TX_ABORTING);
    assert(!h9_aprs_tx_service_submit(&service, packet, sizeof packet));
    assert(!h9_aprs_tx_service_next_sample(&service, &sample));
    assert(service.phase == H9_APRS_TX_IDLE && service.wave.active == 0u);
    assert(!h9_aprs_tx_service_take_encoded_event(&service));

    h9_aprs_tx_service_set_timing(&service, 70u, 40u);
    assert(h9_aprs_tx_service_submit(&service, packet, sizeof packet));
    for (attempt = 0; attempt < 8u &&
         service.phase == H9_APRS_TX_WAITING; ++attempt)
        (void)h9_aprs_tx_service_step(&service, false, false, false, 255u);
    assert(service.phase == H9_APRS_TX_STARTING);
    assert(h9_aprs_tx_service_next_sample(&service, &sample));
    assert(service.wave.lead_ms == 70u && service.wave.tail_ms == 40u);
    assert(h9_aprs_tx_service_cancel(&service) == H9_APRS_TX_ABORTING);
    assert(!h9_aprs_tx_service_next_sample(&service, &sample));
    assert(service.phase == H9_APRS_TX_IDLE && service.wave.active == 0u);
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
                                   false, false, 255u) == H9_APRS_TX_STARTING);
    assert(h9_aprs_tx_runtime_wave_active(&fake.runtime));
    h9_aprs_controller_adc_tick(&controller);
    assert(fake.sample_calls == 1u);
    assert(h9_aprs_tx_runtime_step(&fake.runtime, false, true,
                                   false, false, 255u) == H9_APRS_TX_ABORTING);
    assert(h9_aprs_tx_runtime_wave_active(&fake.runtime));
    h9_aprs_controller_adc_tick(&controller);
    assert(fake.service.phase == H9_APRS_TX_IDLE);
    assert(fake.service.wave.active == 0u);
    h9_aprs_controller_adc_tick(&controller);
    assert(fake.dac == 0x80u && fake.sample_calls == 2u);
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
    H9AprsTxService service;
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
    hw.tx_settings_changed = capture_tx_settings;
    hw.read_adc = read_adc;
    hw.demod_sample = demod;
    hw.read_dac_port = read_dac;
    hw.write_dac_port = write_dac;
    hw.tx_wave_active = tx_active;
    hw.next_tx_wave_sample = next_tx_wave;
    h9_aprs_tx_service_init(&service, 0u,
                            H9_APRS_STOCK_TX_LEAD_MS,
                            H9_APRS_STOCK_TX_TAIL_MS);
    fake.tx_service = &service;
    h9_aprs_controller_init(&controller, &hw);
    assert(controller.tx_settings.lead_ms == 500u &&
           controller.tx_settings.tail_ms == 100u &&
           controller.tx_settings.parameter3_ms == 200u &&
           controller.tx_settings.persistence == 63u);
    /* Stock applies single-byte configuration commands before frame end. */
    h9_aprs_controller_uart_byte(&controller, 0xC0u);
    h9_aprs_controller_uart_byte(&controller, 0x31u); /* low-nibble alias */
    h9_aprs_controller_uart_byte(&controller, 42u);
    assert(controller.tx_settings.lead_ms == 420u &&
           service.pending_timing_ms ==
               ((uint32_t)H9_APRS_STOCK_TX_TAIL_MS << 16 | 420u) &&
           fake.tx_settings_changes == 1u);
    h9_aprs_controller_uart_byte(&controller, 0xDBu); /* raw, not escape */
    assert(controller.tx_settings.lead_ms == 2190u &&
           (uint16_t)service.pending_timing_ms == 2190u);
    h9_aprs_controller_uart_byte(&controller, 0xC0u);
    h9_aprs_controller_uart_byte(&controller, 0xC0u);
    h9_aprs_controller_uart_byte(&controller, 4u);
    h9_aprs_controller_uart_byte(&controller, 12u);
    assert(controller.tx_settings.tail_ms == 120u &&
           (uint16_t)(service.pending_timing_ms >> 16) == 120u);
    h9_aprs_controller_uart_byte(&controller, 0xC0u);
    h9_aprs_controller_uart_byte(&controller, 0xC0u);
    h9_aprs_controller_uart_byte(&controller, 3u);
    h9_aprs_controller_uart_byte(&controller, 25u);
    assert(controller.tx_settings.parameter3_ms == 250u);
    h9_aprs_controller_uart_byte(&controller, 0xC0u);
    h9_aprs_controller_uart_byte(&controller, 0xC0u);
    h9_aprs_controller_uart_byte(&controller, 2u);
    h9_aprs_controller_uart_byte(&controller, 17u);
    assert(controller.tx_settings.persistence == 17u &&
           fake.tx_settings.persistence == 17u &&
           fake.tx_settings_changes == 5u);
    h9_aprs_controller_uart_byte(&controller, 0xC0u);
    assert(!h9_aprs_controller_report_tx_encoded(&controller));
    query = 1u;
    feed_frame(&controller, 0x0Fu, &query, 1u);
    assert(controller.tx_status_enabled);
    assert(h9_aprs_controller_report_tx_encoded(&controller));
    h9_main_wire_reset(&main_receiver);
    for (i = 0; i < fake.uart_length; ++i)
        (void)h9_main_wire_feed(&main_receiver, fake.uart[i]);
    assert(main_receiver.frame_ready && main_receiver.command == 0x0Fu &&
           main_receiver.length == 1u && main_receiver.payload[0] == 1u);
    fake.uart_length = 0u;
    query = 0u;
    feed_frame(&controller, 0x0Fu, &query, 1u);
    assert(!controller.tx_status_enabled &&
           !h9_aprs_controller_report_tx_encoded(&controller) &&
           fake.uart_length == 0u);
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
    query = 1u;
    feed_frame(&controller, 0x20u, &query, 1);
    assert(fake.uart_length == 12);
    assert(fake.uart[0] == 0xC0u && fake.uart[1] == 0x21u);
    assert(fake.uart[2] == 1 && memcmp(fake.uart + 3, "H9R0.01", 7) == 0);
    h9_main_wire_reset(&main_receiver);
    for (i = 0; i < fake.uart_length - 1; ++i)
        assert(h9_main_wire_feed(&main_receiver, fake.uart[i]) ==
               H9_MAIN_WIRE_MORE);
    assert(h9_main_wire_feed(&main_receiver, fake.uart[i]) ==
           H9_MAIN_WIRE_FRAME);
    assert(main_receiver.command == 0x21u && main_receiver.length == 9u);
    assert(main_receiver.payload[0] == 1u);
    assert(memcmp(main_receiver.payload + 1, "H9R0.01", 7) == 0);
    assert(main_receiver.payload[8] == 0);
    h9_main_aprs_versions_init(&versions);
    assert(h9_main_aprs_version_from_frame(&main_receiver, &versions));
    assert(versions.has_module && !versions.has_protocol);
    assert(strcmp(versions.module, "H9R0.01") == 0);
    fake.uart_length = 0;
    query = 0;
    feed_frame(&controller, 0x20u, &query, 1);
    h9_main_wire_reset(&main_receiver);
    for (i = 0; i < fake.uart_length; ++i)
        (void)h9_main_wire_feed(&main_receiver, fake.uart[i]);
    assert(main_receiver.frame_ready);
    assert(h9_main_aprs_version_from_frame(&main_receiver, &versions));
    assert(versions.has_module && versions.has_protocol);
    assert(strcmp(versions.module, "H9R0.01") == 0);
    assert(strcmp(versions.protocol, "V1.0.0") == 0);
    main_receiver.length = 2;
    main_receiver.payload[0] = 1;
    main_receiver.payload[1] = 'X';
    assert(!h9_main_aprs_version_from_frame(&main_receiver, &versions));
    assert(strcmp(versions.module, "H9R0.01") == 0);
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
    assert(controller.valid_rx_frames == 1u &&
           controller.forwarded_rx_frames == 1u);
    assert(fake.uart[0] == 0xC0u && fake.uart[1] == 0 &&
           fake.uart[20] == 0xC0u);
    for (i = 0; i < 18; ++i) assert(fake.uart[i + 2] == (uint8_t)i);
    controller.hardware.uart_write = NULL;
    assert(h9_aprs_ring_push(&controller.demod.receive, 0x7Eu));
    for (i = 0; i < 18; ++i)
        assert(h9_aprs_ring_push(&controller.demod.receive, (uint8_t)i));
    assert(h9_aprs_ring_push(&controller.demod.receive, 0x45u));
    assert(h9_aprs_ring_push(&controller.demod.receive, 0x87u));
    assert(h9_aprs_ring_push(&controller.demod.receive, 0x7Eu));
    h9_aprs_controller_process_received(&controller);
    assert(controller.valid_rx_frames == 2u &&
           controller.forwarded_rx_frames == 1u);
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
    assert(driver.completed_batches == 1u &&
           driver.last_batch_length == 2u);
    assert(h9_aprs_uart_receive_process(&receive, &controller,
            h9_aprs_uart1_dma_rearm, &driver) == 2u);
    assert(receive.current == 1u && dma_channel[1] == 512u &&
           dma_channel[3] ==
               (uint32_t)(uintptr_t)receive.buffers[1]);

    /* Empty UART IDLE immediately rearms the same buffer. */
    h9_aprs_uart1_irq(&driver);
    assert(receive.ready == 0u && receive.current == 1u &&
           dma_channel[1] == 512u && (dma_channel[0] & 1u) != 0);
    assert(driver.completed_batches == 1u);

    /* An invalid DMA remainder cannot strand the UART with DMA stopped. */
    dma_channel[1] = 513u;
    h9_aprs_uart1_irq(&driver);
    assert(driver.rejected_batches == 1u && receive.ready == 0u &&
           dma_channel[1] == 512u && (dma_channel[0] & 1u) != 0);

    /* A full DMA buffer is published even without a following IDLE. */
    dma_controller[0] = 0x02u;
    h9_aprs_dma1_channel1_irq(&driver);
    assert(receive.ready == 1u && receive.pending_length == 512u &&
           dma_controller[1] == 0x0Fu);
    assert(driver.completed_batches == 2u &&
           driver.last_batch_length == 512u);
    assert(h9_aprs_uart_receive_process(&receive, &controller,
            h9_aprs_uart1_dma_rearm, &driver) == 512u);

    dma_controller[0] = 0x08u;
    h9_aprs_dma1_channel1_irq(&driver);
    assert(receive.ready == 0u && dma_channel[1] == 512u &&
           (dma_channel[0] & 1u) != 0);
    assert(driver.transfer_errors == 1u);
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

static void test_aprs_uart2_recover_after_raw_timeout(void)
{
    H9AprsUart2Board board = { 0 };
    H9AprsUart2Dma dma = { 0 };
    H9AprsUart2Rx receive = { 0 };
    uint32_t rcc[17] = { 0 };
    uint32_t gpio[9] = { 0 };
    uint32_t usart[6] = { 0 };
    uint32_t dma_controller[2] = { 0 };
    uint32_t channel[4] = { 0 };
    uint32_t request = 0u;
    uint32_t nvic = 0u;
    const uint8_t byte = 0x42u;

    h9_aprs_uart2_board_bind(&board, rcc, gpio, usart, 2u);
    h9_aprs_uart2_dma_bind(&dma, usart, dma_controller, channel,
                            &request, rcc, &nvic, &receive);
    assert(h9_aprs_uart2_board_configure(&board, 24000000u));
    assert(h9_aprs_uart2_dma_start(&dma));
    assert((channel[0] & 1u) != 0u);
    assert(h9_aprs_uart2_board_write(&board, &byte, 1u) == 0u);
    assert(board.faulted && board.timeouts == 1u);
    receive.ready = true;
    assert(!h9_aprs_uart2_board_recover(&board, &dma, 24000000u));
    receive.ready = false;
    dma.spill_active = true;
    assert(!h9_aprs_uart2_board_recover(&board, &dma, 24000000u));
    dma.spill_active = false;
    assert(!h9_aprs_uart2_board_recover(&board, &dma, 0u));
    assert(board.faulted && (channel[0] & 1u) != 0u);
    assert(board.recovery_failures == 0u && board.recovery_successes == 0u);
    dma.dma_controller = NULL;
    assert(!h9_aprs_uart2_board_recover(&board, &dma, 24000000u));
    assert(board.recovery_failures == 1u && board.recovery_successes == 0u);
    dma.dma_controller = dma_controller;
    assert(h9_aprs_uart2_board_recover(&board, &dma, 24000000u));
    assert(!board.faulted && board.timeouts == 1u &&
           board.recovery_failures == 1u && board.recovery_successes == 1u &&
           (channel[0] & 1u) != 0u &&
           channel[1] == H9_APRS_UART2_BATCH_CAPACITY &&
           (usart[3] & 0x10u) != 0u);
    usart[0] = 0xC0u;
    assert(h9_aprs_uart2_board_write(&board, &byte, 1u) == 1u);
    assert(usart[1] == byte && board.bytes_completed == 1u);
    assert(!h9_aprs_uart2_board_recover(&board, &dma, 24000000u));
    assert(board.recovery_failures == 1u && board.recovery_successes == 1u);
}

static void test_aprs_runtime_status_recovery_counters(void)
{
    H9AprsRuntimeState state = { 0 };
    uint8_t output[H9_APRS_STATUS_BYTES];
    state.uart1_dma.completed_batches = 7u;
    state.uart2_board.recovery_failures = 0x12345678u;
    state.uart2_board.recovery_successes = 0x9ABCDEF0u;
    state.last_uart1_batch_tick = 900u;
    state.systick.ticks = 1000u;
    assert(!h9_aprs_runtime_status_read(NULL, output));
    assert(!h9_aprs_runtime_status_read(&state, NULL));
    assert(h9_aprs_runtime_status_read(&state, output));
    assert(output[0] == 7u && output[1] == 0u);
    assert(output[16] == 0x78u && output[17] == 0x56u &&
           output[18] == 0x34u && output[19] == 0x12u);
    assert(output[20] == 0xF0u && output[21] == 0xDEu &&
           output[22] == 0xBCu && output[23] == 0x9Au);
    assert(output[46] == 100u && output[47] == 0u);
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
    for (index = 0; index < H9_APRS_UART2_BATCH_CAPACITY; ++index) {
        usart[0] = 0x20u;
        usart[1] = 'A';
        h9_aprs_uart2_rx_irq(&rx);
    }
    assert(rx.ready && rx.length == H9_APRS_UART2_BATCH_CAPACITY);
    assert(h9_aprs_uart2_rx_poll(&rx, &controller));
    assert(fake.uart_length == H9_APRS_MAX_SEND + 3u);
    assert(rx.dropped_count == 4u);
    assert(rx.length == 0u && !rx.ready);

    /* A failed serial forward must account for the entire discarded batch. */
    controller.hardware.uart_write = NULL;
    usart[0] = 0x20u;
    usart[1] = 'Q';
    h9_aprs_uart2_rx_irq(&rx);
    usart[0] = 0x10u;
    h9_aprs_uart2_rx_irq(&rx);
    assert(rx.ready && rx.length == 1u);
    assert(!h9_aprs_uart2_rx_poll(&rx, &controller));
    assert(rx.dropped_count == 5u && !rx.ready);
}

static void test_aprs_uart2_dma_forward(void)
{
    H9AprsUart2Rx rx = { 0 };
    H9AprsUart2Dma dma = { 0 };
    H9AprsUart2Dma target = { 0 };
    H9AprsController controller;
    H9AprsHardware hardware = { 0 };
    H9WireDecoder decoded;
    FakeHardware fake = { 0 };
    uint32_t usart[6] = { 0 };
    uint32_t dma_controller[2] = { 0 };
    uint32_t channel[4] = { 0 };
    uint32_t dma_request = 0xA5C3u;
    uint32_t rcc[17] = { 0 };
    uint32_t nvic = 0u;
    size_t index;
    hardware.context = &fake;
    hardware.uart_write = uart_write;
    h9_aprs_controller_init(&controller, &hardware);
    h9_aprs_uart2_rx_bind(&rx, usart, &nvic);
    h9_aprs_uart2_dma_bind(&dma, usart, dma_controller, channel,
                            &dma_request,
                            rcc, &nvic, &rx);
    h9_aprs_uart2_dma_bind_target(&target, &rx);
    assert((uintptr_t)target.dma_channel == 0x40020030u);
    assert((uintptr_t)target.dma_request == 0x4001001Cu);
    assert(h9_aprs_uart2_dma_start(&dma));
    assert((rcc[0x38u / 4u] & 1u) != 0u &&
           (rcc[0x40u / 4u] & 1u) != 0u &&
           (dma_request & (0x1Fu << 16)) == (8u << 16) &&
           (dma_request & ~(0x1Fu << 16)) == 0xA5C3u &&
           channel[0] == 0x108Bu &&
           channel[1] == H9_APRS_UART2_BATCH_CAPACITY &&
           channel[2] == (uint32_t)(uintptr_t)&usart[1] &&
           channel[3] == (uint32_t)(uintptr_t)rx.bytes);
    assert((usart[3] & 0x10u) != 0u &&
           (usart[3] & 0x20u) == 0u &&
           (usart[5] & 0x40u) != 0u &&
           nvic == ((1u << 10) | (1u << 28)));
    rx.bytes[0] = 'G';
    rx.bytes[1] = 'P';
    channel[1] = H9_APRS_UART2_BATCH_CAPACITY - 2u;
    usart[0] = 0x10u;
    h9_aprs_uart2_dma_usart_irq(&dma);
    assert(rx.ready && rx.length == 2u && (channel[0] & 1u) == 0u &&
           (usart[5] & 0x40u) == 0u);
    assert(h9_aprs_uart2_dma_poll(&dma, &controller));
    assert(!rx.ready && rx.length == 0u &&
           channel[1] == H9_APRS_UART2_BATCH_CAPACITY &&
           (channel[0] & 1u) != 0u);
    h9_wire_reset(&decoded);
    for (index = 0u; index < fake.uart_length; ++index)
        (void)h9_wire_feed(&decoded, fake.uart[index]);
    assert(decoded.command == 0x10u && decoded.length == 2u &&
           decoded.payload[0] == 'G' && decoded.payload[1] == 'P');

    /* USART2 bytes arriving while USART1 forwards a DMA batch are retained. */
    fake.uart_length = 0u;
    fake.inject_uart2_dma = &dma;
    fake.inject_uart2_registers = usart;
    fake.inject_uart2_once = true;
    rx.bytes[0] = 'A';
    channel[1] = H9_APRS_UART2_BATCH_CAPACITY - 1u;
    usart[0] = 0x10u;
    h9_aprs_uart2_dma_usart_irq(&dma);
    assert(rx.ready && dma.spill_active && (usart[3] & 0x20u) != 0u);
    assert(h9_aprs_uart2_dma_poll(&dma, &controller));
    assert(!fake.inject_uart2_once && !dma.spill_active &&
           dma.spill_head == dma.spill_tail &&
           (channel[0] & 1u) != 0u);
    h9_wire_reset(&decoded);
    index = 0u;
    while (index < fake.uart_length &&
           h9_wire_feed(&decoded, fake.uart[index++]) != H9_WIRE_FRAME) { }
    assert(decoded.command == 0x10u && decoded.length == 1u &&
           decoded.payload[0] == 'A');
    h9_wire_reset(&decoded);
    while (index < fake.uart_length &&
           h9_wire_feed(&decoded, fake.uart[index++]) != H9_WIRE_FRAME) { }
    assert(decoded.command == 0x10u && decoded.length == 2u &&
           decoded.payload[0] == 'B' && decoded.payload[1] == 'C');
    fake.inject_uart2_dma = NULL;
    fake.inject_uart2_registers = NULL;

    /* Full spill ring drops exactly one excess byte and still rearms DMA. */
    fake.uart_length = 0u;
    rx.bytes[0] = 'Z';
    channel[1] = H9_APRS_UART2_BATCH_CAPACITY - 1u;
    usart[0] = 0x10u;
    h9_aprs_uart2_dma_usart_irq(&dma);
    for (index = 0u; index < 256u; ++index) {
        usart[0] = 0x20u;
        usart[1] = (uint32_t)index;
        h9_aprs_uart2_dma_usart_irq(&dma);
        usart[0] = 0u;
    }
    assert(dma.spill_drops == 1u && rx.dropped_count == 1u);
    assert(h9_aprs_uart2_dma_poll(&dma, &controller));
    assert(!rx.ready && !dma.spill_active &&
           dma.spill_head == dma.spill_tail &&
           channel[1] == H9_APRS_UART2_BATCH_CAPACITY);

    fake.uart_length = 0u;
    memset(rx.bytes, 'A', sizeof rx.bytes);
    dma_controller[0] = 0x200u;
    h9_aprs_uart2_dma_channel_irq(&dma);
    assert(rx.ready && rx.length == H9_APRS_UART2_BATCH_CAPACITY);
    assert(h9_aprs_uart2_dma_poll(&dma, &controller));
    assert(fake.uart_length == H9_APRS_MAX_SEND + 3u &&
           rx.dropped_count == 5u);

    dma_controller[0] = 0x800u;
    h9_aprs_uart2_dma_channel_irq(&dma);
    assert(dma.transfer_errors == 1u && !rx.ready &&
           channel[1] == H9_APRS_UART2_BATCH_CAPACITY);
    dma_controller[0] = 0u;
    channel[1] = H9_APRS_UART2_BATCH_CAPACITY + 1u;
    usart[0] = 0x10u;
    h9_aprs_uart2_dma_usart_irq(&dma);
    assert(dma.invalid_counts == 1u && !rx.ready &&
           channel[1] == H9_APRS_UART2_BATCH_CAPACITY);

    /* The DMA path must account for a failed full-batch forward too. */
    controller.hardware.uart_write = NULL;
    rx.bytes[0] = 'Q';
    rx.bytes[1] = 'R';
    channel[1] = H9_APRS_UART2_BATCH_CAPACITY - 2u;
    usart[0] = 0x10u;
    h9_aprs_uart2_dma_usart_irq(&dma);
    assert(rx.ready && rx.length == 2u);
    assert(!h9_aprs_uart2_dma_poll(&dma, &controller));
    assert(rx.dropped_count == 7u && !rx.ready &&
           channel[1] == H9_APRS_UART2_BATCH_CAPACITY);
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
    uint32_t gpio[11] = { 0 };
    assert(h9_aprs_wave_output_read(&output) == 0u);
    h9_aprs_wave_output_bind(&output, rcc, gpio);
    gpio[0] = gpio[1] = gpio[2] = gpio[3] = ~0u;
    h9_aprs_wave_output_configure(&output);
    assert(rcc[13] == 1u);
    assert((gpio[0] & 3u) == 1u);
    assert((gpio[1] & 1u) == 0u);
    assert((gpio[2] & 3u) == 0u);
    assert((gpio[3] & 3u) == 0u);
    assert(gpio[10] == 1u);
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
    assert(fake.demod_calls == 0u && gpio[5] == 0x1Au &&
           controller.adc_sample_count == 0u);
    adc[1] = 8u;
    assert(!h9_aprs_adc_board_sample_irq(&fake.adc, &controller));
    adc[1] = 4u;
    assert(h9_aprs_adc_board_sample_irq(&fake.adc, &controller));
    assert(controller.adc_sample_count == 1u);
    assert(fake.demod_calls == 1u && gpio[5] == 0x8Au);
    assert(adc[0] == 0xCu);
}

static void test_aprs_adc_full_irq(void)
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
    adc[2] = 4u;
    adc[3] = 0x5C04u;
    adc[0] = adc[1] = 0x9Cu;
    fake.adc.state_word_28 = 1u;
    assert(h9_aprs_adc_board_irq(&fake.adc, &controller) == 0x9Cu);
    assert(fake.demod_calls == 1u);
    assert(fake.adc.irq_flags == 0x10200u &&
           fake.adc.event_flags == 2u);
    assert(adc[0] == 0x10u);

    fake.demod_calls = 0u;
    adc[0] = adc[1] = 8u;
    adc[3] = 0u;
    assert(h9_aprs_adc_board_irq(&fake.adc, &controller) == 8u);
    assert(fake.demod_calls == 1u);
    assert((fake.adc.irq_flags & 0x20u) != 0u &&
           fake.adc.event_flags == 3u);

    fake.demod_calls = 0u;
    adc[0] = adc[1] = 8u;
    adc[2] = 0u;
    fake.adc.irq_flags |= 0x100u;
    assert(h9_aprs_adc_board_irq(&fake.adc, &controller) == 8u);
    assert(fake.demod_calls == 1u && adc[1] == 0u);
    assert((fake.adc.irq_flags & 0x100u) == 0u &&
           (fake.adc.irq_flags & 1u) != 0u);
    assert(h9_aprs_adc_board_irq(&fake.adc, &controller) == 0u);
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
    return 0x5678u;
}

static uint32_t boot_record_check_updates_header(const volatile uint8_t *data,
                                                  size_t length, void *context)
{
    volatile uint8_t *header = (volatile uint8_t *)data - 2;
    (void)context;
    assert(length == H9_MAIN_BOOT_RECORD_CHECK_BYTES);
    header[0] = 0x78u;
    header[1] = 0x56u;
    return 0x5678u;
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

typedef struct {
    volatile uint32_t *stream_data;
    volatile uint32_t *stream_control;
    volatile uint32_t *result_word;
    unsigned events;
} BootValidatorProbe;

static void boot_probe_begin(void *context, uint32_t descriptor, uint32_t mode)
{
    BootValidatorProbe *probe = (BootValidatorProbe *)context;
    assert(probe->events == 0u);
    assert(descriptor == H9_MAIN_BOOT_DESCRIPTOR && mode == 0u);
    probe->events = 1u;
}

static void boot_probe_sync(void *context)
{
    BootValidatorProbe *probe = (BootValidatorProbe *)context;
    assert(probe->events == 1u);
    assert(*probe->stream_control == 0u);
    assert(*probe->stream_data == 0xA5u);
    probe->events = 2u;
}

static void boot_probe_finish(void *context, uint32_t descriptor)
{
    BootValidatorProbe *probe = (BootValidatorProbe *)context;
    assert(probe->events == 2u && descriptor == H9_MAIN_BOOT_DESCRIPTOR);
    *probe->result_word = 0u;
    probe->events = 3u;
}

static void test_main_boot_validator(void)
{
    uint8_t record[H9_MAIN_BOOT_RECORD_BYTES];
    volatile uint32_t stream_data = 0u;
    volatile uint32_t stream_control = 0xFFFFFFFFu;
    volatile uint32_t result_word = 0xABCD5678u;
    BootValidatorProbe probe = {
        &stream_data, &stream_control, &result_word, 0u
    };
    H9MainBootValidator validator = {
        &stream_data, &stream_control, &result_word,
        boot_probe_begin, boot_probe_sync, boot_probe_finish, &probe
    };
    size_t index;

    memset(record, 0xA5, sizeof record);
    record[0] = 0x78u; record[1] = 0x56u;
    record[4] = 0xCDu; record[5] = 0xABu;
    record[6] = 0xEFu; record[7] = 0xBEu;
    assert(h9_main_consume_boot_record(record, h9_main_boot_validate,
                                       &validator) == 0xBEEFABCDu);
    assert(probe.events == 3u && result_word == 0u);
    for (index = 0; index < sizeof record; ++index) assert(record[index] == 0u);

    validator.begin = NULL;
    stream_control = 0xFFFFFFFFu;
    assert(h9_main_boot_validate(record + 2,
                                 H9_MAIN_BOOT_RECORD_CHECK_BYTES,
                                 &validator) == 0u);
    assert(stream_control == 0xFFFFFFFFu && probe.events == 3u);
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

static void test_main_serial_bus(void)
{
    volatile uint32_t registers[2] = { 0u, 0u };
    H9MainSerialBus bus = { 0 };
    uint8_t received = 0xA5u;

    h9_main_serial_bus_bind(&bus, registers, 3u);
    assert(h9_main_serial_bus_transfer(&bus, 0x1234u, &received));
    assert(registers[0] == 0x10u && registers[1] == 0x1234u);
    assert(received == 0x34u);

    registers[0] = 2u;
    received = 0xA5u;
    assert(!h9_main_serial_bus_transfer(&bus, 0x5678u, &received));
    assert(registers[0] == 0x12u && registers[1] == 0x5678u);
    assert(received == 0xA5u);

    h9_main_serial_bus_bind(&bus, registers, 0u);
    assert(!h9_main_serial_bus_transfer(&bus, 0x9999u, &received));
    assert(registers[1] == 0x5678u);

    h9_main_serial_bus_bind_target(&bus, 1u);
    assert((uintptr_t)bus.registers == 0x001E3E08u);
    assert(!h9_main_serial_bus_transfer(NULL, 0u, &received));
    assert(!h9_main_serial_bus_transfer(&bus, 0u, NULL));
}

typedef struct {
    volatile uint32_t control;
    uint32_t words[3];
    unsigned calls;
    unsigned begins;
    unsigned ends;
    unsigned fail_call;
    bool refuse_begin;
    bool fail_end;
} FakeMainSerialCommands;

static bool main_serial_begin(void *context)
{
    FakeMainSerialCommands *fake = (FakeMainSerialCommands *)context;
    ++fake->begins;
    return !fake->refuse_begin;
}

static bool main_serial_end(void *context)
{
    FakeMainSerialCommands *fake = (FakeMainSerialCommands *)context;
    assert((fake->control & 0x101u) == 0u);
    ++fake->ends;
    return !fake->fail_end;
}

static bool main_serial_exchange(void *context, uint32_t word,
                                  uint8_t *received)
{
    FakeMainSerialCommands *fake = (FakeMainSerialCommands *)context;
    assert((fake->control & 0x101u) == 0x101u);
    assert(fake->calls < 3u && received != NULL);
    fake->words[fake->calls++] = word;
    *received = 0xA5u;
    return fake->calls != fake->fail_call;
}

static void test_main_serial_commands(void)
{
    FakeMainSerialCommands fake = { 0 };
    H9MainSerialCommands commands = {
        &fake.control, main_serial_begin, main_serial_end,
        main_serial_exchange, &fake
    };
    uint8_t received = 0u;
    fake.control = 0x900u;
    assert(h9_main_serial_command_read(&commands, 0x1234u, &received));
    assert(fake.begins == 1u && fake.ends == 1u && fake.calls == 3u);
    assert(fake.words[0] == 0x82u && fake.words[1] == 0x34u &&
           fake.words[2] == 0x5Eu && received == 0xA5u);
    assert(fake.control == 0x800u);

    fake.calls = 0u;
    fake.control = 0x800u;
    assert(h9_main_serial_command_write(&commands, 0x8234u, 0x5Au));
    assert(fake.begins == 2u && fake.ends == 2u && fake.calls == 3u);
    assert(fake.words[0] == 2u && fake.words[1] == 0x34u &&
           fake.words[2] == 0x5Au && fake.control == 0x800u);

    fake.calls = 0u;
    fake.fail_call = 2u;
    assert(!h9_main_serial_command_read(&commands, 0x1234u, &received));
    assert(fake.calls == 2u && fake.ends == 3u && fake.control == 0x800u);
    fake.refuse_begin = true;
    assert(!h9_main_serial_command_write(&commands, 0x1234u, 0u));
    assert(fake.ends == 3u && fake.calls == 2u);
    fake.refuse_begin = false;
    fake.fail_call = 0u;
    fake.fail_end = true;
    fake.calls = 0u;
    assert(!h9_main_serial_command_write(&commands, 0x1234u, 0u));
    assert(fake.ends == 4u && fake.calls == 3u);
    assert(!h9_main_serial_command_read(&commands, 0u, NULL));
    assert(!h9_main_serial_command_read(NULL, 0u, &received));
}

static void test_main_serial_port(void)
{
    volatile uint32_t bus_registers[2] = { 0u, 0u };
    volatile uint32_t gate_control = 0u;
    volatile uint32_t gate_users = 0u;
    H9MainSerialPort port = { 0 };
    uint8_t received = 0u;
    h9_main_serial_port_bind(&port, bus_registers, &gate_control,
                              &gate_users, 3u);
    assert(h9_main_serial_gate_acquire(&port.gate));
    assert(h9_main_serial_gate_acquire(&port.gate));
    assert(gate_control == 6u && gate_users == 2u);
    assert(h9_main_serial_gate_release(&port.gate));
    assert(gate_control == 6u && gate_users == 1u);
    assert(h9_main_serial_gate_release(&port.gate));
    assert(gate_control == 0u && gate_users == 0u);
    assert(!h9_main_serial_gate_release(&port.gate));
    gate_users = UINT32_MAX;
    assert(!h9_main_serial_gate_acquire(&port.gate));
    assert(gate_control == 0u && gate_users == UINT32_MAX);
    gate_users = 0u;

    assert(h9_main_serial_command_read(&port.commands, 0x1234u,
                                        &received));
    assert(received == 0x5Eu && bus_registers[1] == 0x5Eu);
    assert(gate_control == 0u && gate_users == 0u);
    assert((bus_registers[0] & 0x101u) == 0u);
    assert(h9_main_serial_command_write(&port.commands, 0x8234u, 0xA5u));
    assert(bus_registers[1] == 0xA5u &&
           gate_control == 0u && gate_users == 0u);

    bus_registers[0] = 2u;
    assert(!h9_main_serial_command_read(&port.commands, 0x1234u,
                                         &received));
    assert(gate_control == 0u && gate_users == 0u);
    assert((bus_registers[0] & 0x101u) == 0u);
    h9_main_serial_port_bind_target(&port, 5u);
    assert((uintptr_t)port.bus.registers == 0x001E3E08u &&
           (uintptr_t)port.gate.control == 0x0010F1A8u &&
           (uintptr_t)port.gate.users == 0x0001176Cu);
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

typedef struct {
    uint32_t *registers;
    unsigned prepares;
    unsigned enables;
    unsigned tx_acks;
    unsigned rx_acks;
} MainUartLinkCapture;

static void main_uart_link_prepare(void *context, uint32_t descriptor)
{
    MainUartLinkCapture *capture = (MainUartLinkCapture *)context;
    assert(descriptor == 0x001E3B0u);
    capture->prepares++;
}

static void main_uart_link_enable(void *context, uint32_t descriptor,
                                   uint32_t option)
{
    MainUartLinkCapture *capture = (MainUartLinkCapture *)context;
    assert(descriptor == 0x001E3B0u && option == 0u);
    assert(capture->registers[5] == 0x12000u);
    assert(capture->registers[6] == 11u);
    capture->enables++;
}

static void main_uart_link_tx_ack(void *context)
{
    ((MainUartLinkCapture *)context)->tx_acks++;
}

static void main_uart_link_rx_ack(void *context)
{
    ((MainUartLinkCapture *)context)->rx_acks++;
}

static void test_main_aprs_uart1_link(void)
{
    uint32_t registers[11] = { 0 };
    uint8_t receive[32] = { 0 };
    uint8_t outgoing[32] = { 0 };
    H9MainUart1TxSlot slot = { outgoing, 0x12000u,
                               sizeof outgoing, 0u };
    MainUartLinkCapture capture = { registers, 0u, 0u, 0u, 0u };
    H9MainAprsUart1DmaHooks hooks = {
        main_uart_link_prepare, main_uart_link_enable,
        main_uart_link_tx_ack, main_uart_link_rx_ack, &capture
    };
    H9MainAprsUart1Link link;
    static const uint8_t gpio_query[] = { 3u };
    size_t query_length = h9_wire_encode(0x11u, gpio_query,
                                          sizeof gpio_query,
                                          receive, sizeof receive);
    assert(query_length != 0u);
    assert(!h9_main_aprs_uart1_link_init(&link, registers, receive,
                                         31u, (uint32_t)query_length,
                                         &slot, 1u, &hooks, NULL, NULL));
    assert(h9_main_aprs_uart1_link_init(&link, registers, receive,
                                        sizeof receive,
                                        (uint32_t)query_length, &slot, 1u,
                                        &hooks, NULL, NULL));
    registers[0] = 0x4008u;
    h9_main_aprs_uart1_link_irq(&link);
    assert(capture.rx_acks == 1u && link.receive.produced == query_length);
    assert(h9_main_aprs_uart1_link_poll(&link) == query_length);
    assert(link.receive.consumed == query_length);
    assert(capture.prepares == 1u && capture.enables == 1u);
    assert(link.transmit_queue.active && outgoing[0] == 0xC0u &&
           outgoing[10] == 0xC0u);
    registers[0] = 0x8004u;
    h9_main_aprs_uart1_link_irq(&link);
    assert(capture.tx_acks == 1u && link.transmit_queue.completed == 1u);
    assert(h9_main_aprs_uart1_link_poll(&link) == 0u);
    assert(!link.transmit_queue.active && link.transmit_queue.count == 0u);
}

static void test_main_aprs_uart1_link_forwards_gps_fix(void)
{
    uint32_t registers[11] = { 0 };
    uint8_t receive[256] = { 0 };
    uint8_t outgoing[32] = { 0 };
    uint8_t nmea[160];
    H9MainUart1TxSlot slot = { outgoing, 0x12000u,
                               sizeof outgoing, 0u };
    MainUartLinkCapture dma_capture = { registers, 0u, 0u, 0u, 0u };
    MainSessionCapture application_capture = { 0 };
    H9MainAprsUart1DmaHooks hooks = {
        main_uart_link_prepare, main_uart_link_enable,
        main_uart_link_tx_ack, main_uart_link_rx_ack, &dma_capture
    };
    H9MainAprsSessionCallbacks application = { 0 };
    H9MainAprsUart1Link link;
    size_t nmea_length = make_nmea(
        "GNRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,231126,,,A",
        nmea, sizeof nmea);
    size_t frame_length = h9_wire_encode(0x10u, nmea, nmea_length,
                                          receive, sizeof receive);

    assert(nmea_length != 0u && frame_length != 0u);
    application.gps_fix = session_gps_fix;
    assert(h9_main_aprs_uart1_link_init(&link, registers, receive,
                                        sizeof receive, 128u, &slot, 1u,
                                        &hooks, &application,
                                        &application_capture));
    registers[0] = 0x0820u;
    registers[10] = (uint32_t)frame_length;
    h9_main_aprs_uart1_link_irq(&link);
    assert(dma_capture.rx_acks == 1u &&
           link.receive.produced == frame_length);
    assert(h9_main_aprs_uart1_link_poll(&link) == frame_length);
    assert(application_capture.gps_fix_count == 1u);
    assert(application_capture.gps_type == H9_MAIN_GPS_RMC);
    assert(application_capture.gps_fix.latitude_e7 == 481173000);
}

typedef struct {
    uint32_t *registers;
    unsigned steps[5];
    unsigned count;
    void (*handler)(void *);
    void *handler_context;
} MainUartBoardCapture;

static void main_uart_board_pins(void *context, uint8_t tx, uint8_t rx)
{
    MainUartBoardCapture *capture = (MainUartBoardCapture *)context;
    assert(tx == 0x20u && rx == 0x21u);
    capture->steps[capture->count++] = 1u;
}

static void main_uart_board_dma(void *context, uint32_t descriptor)
{
    MainUartBoardCapture *capture = (MainUartBoardCapture *)context;
    assert(capture->registers[0] == 0x3400u);
    assert(descriptor == (capture->count == 1u ?
                          0x001E360u : 0x001E3B0u));
    capture->steps[capture->count++] = 2u;
}

static void main_uart_board_irq(void *context, uint8_t number,
                                 uint8_t priority, void (*handler)(void *),
                                 void *handler_context)
{
    MainUartBoardCapture *capture = (MainUartBoardCapture *)context;
    assert(number == 0x13u && priority == 3u && handler != NULL);
    capture->handler = handler;
    capture->handler_context = handler_context;
    capture->steps[capture->count++] = 3u;
}

static void main_uart_board_baud(void *context, uint32_t half_baud)
{
    MainUartBoardCapture *capture = (MainUartBoardCapture *)context;
    assert(half_baud == 9600u);
    assert(capture->registers[0] == 0x3468u);
    assert(capture->registers[7] == 0x14000u);
    assert(capture->registers[8] == 0x14800u);
    assert(capture->registers[9] == 1024u);
    capture->steps[capture->count++] = 4u;
}

static void test_main_uart1_board_open(void)
{
    uint32_t registers[11] = { 0 };
    uint8_t receive[2048] = { 0 };
    uint8_t outgoing[32] = { 0 };
    H9MainUart1TxSlot slot = { outgoing, 0x16000u,
                               sizeof outgoing, 0u };
    MainUartLinkCapture dma_capture = { registers, 0u, 0u, 0u, 0u };
    MainUartBoardCapture board_capture = { registers, { 0 }, 0u,
                                          NULL, NULL };
    H9MainAprsUart1DmaHooks dma = {
        main_uart_link_prepare, main_uart_link_enable,
        main_uart_link_tx_ack, main_uart_link_rx_ack, &dma_capture
    };
    H9MainUart1BoardHooks board = {
        main_uart_board_pins, main_uart_board_dma,
        main_uart_board_irq, main_uart_board_baud, &board_capture
    };
    H9MainSerialOpenRecord record;
    H9MainAprsUart1Link link;

    assert(h9_main_aprs_uart1_link_init(&link, registers, receive,
                                        sizeof receive, 1024u, &slot, 1u,
                                        &dma, NULL, NULL));
    h9_main_aprs_serial_profile(&record, 0x14000u, 0u);
    record.storage_size = 1024u;
    assert(!h9_main_uart1_board_open(&link, &record, &board));
    assert(registers[0] == 0u && board_capture.count == 0u);
    record.storage_size = 2048u;
    registers[0] = 1u;
    assert(!h9_main_uart1_board_open(&link, &record, &board));
    assert(registers[0] == 1u && board_capture.count == 0u);
    registers[0] = 0u;
    assert(h9_main_uart1_board_open(&link, &record, &board));
    assert(board_capture.count == 5u);
    assert(board_capture.steps[0] == 1u &&
           board_capture.steps[1] == 2u &&
           board_capture.steps[2] == 2u &&
           board_capture.steps[3] == 3u &&
           board_capture.steps[4] == 4u);
    assert(board_capture.handler_context == &link);
    registers[0] = 0x4008u;
    board_capture.handler(board_capture.handler_context);
    assert(link.receive.produced == 1024u && dma_capture.rx_acks == 1u);
}

static void test_main_uart1_baud(void)
{
    uint32_t uart[5] = { 0x68u, 0u, 0xA5A5u, 0u, 0xB6B6u };
    uint32_t clock_ready = 0u;
    uint32_t timebase = 1000u;
    uint32_t activity = 5u;
    H9MainUart1Baud baud = { uart, &clock_ready, &timebase, &activity };
    assert(!h9_main_uart1_baud_configure(NULL, 9600u));
    assert(!h9_main_uart1_baud_configure(&baud, 0u));
    assert(!h9_main_uart1_baud_configure(&baud, 9600u));
    assert(uart[0] == 0x68u && uart[2] == 0xA5A5u &&
           uart[4] == 0xB6B6u);

    clock_ready = 48000000u;
    assert(h9_main_uart1_baud_configure(&baud, 9600u));
    assert(uart[0] == 0x3469u && uart[2] == 1249u && uart[4] == 5u);

    uart[0] = 0x40u;
    uart[4] = 0xB6B6u;
    assert(h9_main_uart1_baud_configure(&baud, 9600u));
    assert(uart[0] == 0x3441u && uart[4] == 0xB6B6u);

    uart[0] = 0x68u;
    activity = 25u;
    timebase = 1000u;
    assert(h9_main_uart1_baud_configure(&baud, 9600u));
    assert(uart[4] == 200u);
}

static void test_main_heap_bitmap(void)
{
    uint8_t initialized = 0u;
    uint32_t words[H9_MAIN_HEAP_BITMAP_WORDS];
    H9MainHeapBitmap bitmap = { &initialized, words };
    uint32_t block;
    unsigned index;
    for (index = 0u; index < H9_MAIN_HEAP_BITMAP_WORDS; ++index)
        words[index] = 0xFFFFFFFFu;
    assert(!h9_main_heap_bitmap_initialize(NULL));
    assert(h9_main_heap_bitmap_initialize(&bitmap));
    assert(initialized == 1u);
    for (block = 0u; block < 0x600u; ++block) {
        bool expected = (block >= 0x3CAu && block < 0x57Eu) ||
                        (block >= 0x5A4u && block < 0x5FFu);
        bool actual = (words[block >> 5] & (1u << (block & 31u))) != 0u;
        assert(actual == expected);
    }
    assert(words[48] == 0xC0000000u);
    assert(words[49] == 0xEFFFu);
    assert(words[50] == 0x0001E500u);
    assert(words[51] == 0x0002FF80u);
    assert(words[52] == 0x00010780u);
    assert(words[53] == 0x05FF03CAu);
    assert(words[54] == 0x020F0000u);
    words[0] = 0x12345678u;
    assert(h9_main_heap_bitmap_initialize(&bitmap));
    assert(words[0] == 0x12345678u);
}

static void test_main_memory_window(void)
{
    uint32_t controller[3] = { 0x80u, 0u, 0u };
    uint8_t window_bytes[H9_MAIN_MEMORY_WINDOW_BYTES];
    uint32_t state_words[H9_MAIN_MEMORY_STATE_BYTES / 4u];
    uint8_t *state_bytes = (uint8_t *)state_words;
    H9MainMemoryWindow window = { controller, window_bytes, state_bytes };
    unsigned index;
    memset(window_bytes, 0xA5, sizeof window_bytes);
    memset(state_words, 0xA5, sizeof state_words);
    assert(!h9_main_memory_window_initialize(NULL));
    assert(h9_main_memory_window_initialize(&window));
    assert(controller[0] == 0x81u && controller[1] == 0x2C000u &&
           controller[2] == 0x2D1FFu);
    for (index = 0u; index < sizeof window_bytes; ++index)
        assert(window_bytes[index] == 0u);
    assert(state_bytes[0] == 1u && state_bytes[1] == 0xA5u);
    assert(state_words[1] == 0x2C000u && state_words[2] == 0x248000u);
    for (index = 5u; index < 20u; ++index)
        assert(state_words[index] == 0xFFFFFFFFu);
    assert(state_words[20] == 0x200000u);
    assert(state_words[21] == 0x20F000u);
    assert(state_words[22] == 0xF000u);
    assert(state_words[23] == 0x1E00000u);
    assert(state_bytes[0x60] == 0xE0u && state_bytes[0x61] == 1u &&
           state_bytes[0x62] == 0xA5u);
    for (index = 0x64u; index < 0xDDu; ++index)
        assert(state_bytes[index] == 0u);
    assert(state_bytes[0xDDu] == 0xA5u);
}

static void test_main_board_markers(void)
{
    uint8_t ram_guard[H9_MAIN_GUARD_BYTES + 2u];
    uint8_t stack_guard[H9_MAIN_GUARD_BYTES + 2u];
    uint8_t flags[3] = { 0u, 0u, 0u };
    H9MainBoardMarkers markers = {
        ram_guard + 1u, stack_guard + 1u,
        &flags[0], &flags[1], &flags[2]
    };
    unsigned index;
    memset(ram_guard, 0xA5, sizeof ram_guard);
    memset(stack_guard, 0xA5, sizeof stack_guard);
    assert(!h9_main_board_markers_initialize(NULL));
    assert(h9_main_board_markers_initialize(&markers));
    assert(ram_guard[0] == 0xA5u &&
           ram_guard[H9_MAIN_GUARD_BYTES + 1u] == 0xA5u);
    assert(stack_guard[0] == 0xA5u &&
           stack_guard[H9_MAIN_GUARD_BYTES + 1u] == 0xA5u);
    for (index = 1u; index <= H9_MAIN_GUARD_BYTES; ++index) {
        assert(ram_guard[index] == 0x5Au);
        assert(stack_guard[index] == 0x5Au);
    }
    assert(flags[0] == 1u && flags[1] == 1u && flags[2] == 1u);
}

typedef struct {
    unsigned writes;
    unsigned reads;
    unsigned masks;
    unsigned polls;
    bool ready;
} FakeClockProbe;

static void clock_probe_write(void *context, uint32_t selector,
                              uint32_t value)
{
    FakeClockProbe *fake = (FakeClockProbe *)context;
    unsigned channel = fake->writes / 2u;
    if ((fake->writes & 1u) == 0u)
        assert(selector == 0xB0u && value == 0x40u);
    else
        assert(selector == 0xB1u && value == (0x82u | (channel << 2)));
    ++fake->writes;
}

static uint32_t clock_probe_read(void *context, uint32_t selector)
{
    FakeClockProbe *fake = (FakeClockProbe *)context;
    assert(selector == 0xB2u && fake->writes == (fake->reads + 1u) * 2u);
    ++fake->reads;
    return 0u;
}

static void clock_probe_mask(void *context, uint32_t selector,
                             uint32_t mask)
{
    FakeClockProbe *fake = (FakeClockProbe *)context;
    assert(selector == 0xB1u && mask == 0xFDu &&
           fake->reads == fake->masks + 1u);
    ++fake->masks;
}

static bool clock_probe_complete(void *context,
                                 volatile uint32_t *measurement)
{
    FakeClockProbe *fake = (FakeClockProbe *)context;
    assert(measurement[0] == 1u && measurement[1] == 0u &&
           measurement[2] == 0x960u);
    ++fake->polls;
    return fake->ready && (fake->polls & 1u) == 0u;
}

static void test_main_clock_probe(void)
{
    uint32_t fuse = 0x900u;
    uint32_t calibration[25];
    uint32_t measurement[3] = { 0u, 0u, 0u };
    FakeClockProbe fake = { 0 };
    H9MainClockProbe probe = {
        &fuse, calibration, measurement, &fake,
        clock_probe_write, clock_probe_read, clock_probe_mask,
        clock_probe_complete, 3u
    };
    unsigned index;
    for (index = 0u; index < 25u; ++index)
        calibration[index] = 0xA5A5A5A5u;
    fake.ready = true;
    assert(!h9_main_clock_probe_run(NULL));
    assert(h9_main_clock_probe_run(&probe));
    assert(fake.writes == 8u && fake.reads == 4u &&
           fake.masks == 4u && fake.polls == 8u);
    assert(measurement[0] == 0x4000u);
    for (index = 0u; index < 25u; ++index)
        assert(calibration[index] ==
               ((index == 3u || index == 8u || index == 13u ||
                 index == 18u) ? 50000000u : 0xA5A5A5A5u));

    fuse = 0u;
    fake = (FakeClockProbe){ 0 };
    fake.ready = false;
    probe.poll_limit = 2u;
    calibration[3] = 0xA5A5A5A5u;
    assert(!h9_main_clock_probe_run(&probe));
    assert(fake.writes == 2u && fake.reads == 0u &&
           fake.masks == 0u && fake.polls == 2u);
    assert(measurement[0] == 0x4000u &&
           calibration[3] == 0xA5A5A5A5u);
}

static void test_main_clock_selector(void)
{
    uint32_t registers[3] = { 0u, 0u, 0xA5A5A5A5u };
    uint8_t flag_da58 = 0u;
    uint8_t flag_da5c = 0u;
    H9MainClockSelector selector = {
        registers, &flag_da58, &flag_da5c
    };
    unsigned preserved;
    unsigned flags;
    assert(!h9_main_clock_selector_run(NULL));
    for (preserved = 0u; preserved < 2u; ++preserved) {
        for (flags = 0u; flags < 4u; ++flags) {
            uint32_t expected = 0x331D524Du & 0xFFF807FFu;
            registers[0] = preserved ? 0x04000000u : 0u;
            registers[1] = 0u;
            flag_da58 = (flags & 1u) != 0u;
            flag_da5c = (flags & 2u) != 0u;
            if (preserved) expected |= 0x04000000u;
            if (flag_da58) expected |= 0x00005000u;
            if (flag_da5c) expected |= 0x00050000u;
            assert(h9_main_clock_selector_run(&selector));
            assert(registers[0] == expected);
            assert(registers[1] == 0x1102u);
            assert(registers[2] == 0xA5A5A5A5u);
        }
    }
}

static void test_main_clock_band(void)
{
    uint32_t storage[22];
    uint32_t *thresholds = storage + 1u;
    uint8_t enabled = 1u;
    uint8_t requested = 0u;
    uint8_t selected = 0xA5u;
    H9MainClockBand band = {
        thresholds, &enabled, &requested, &selected
    };
    unsigned index;
    for (index = 0u; index < 22u; ++index)
        storage[index] = 0xA5A5A5A5u;
    thresholds[0] = 100000000u;
    thresholds[5] = 200000000u;
    thresholds[10] = 240000000u;
    thresholds[15] = 300000000u;
    assert(!h9_main_clock_band_select(NULL, 240000000u));
    assert(h9_main_clock_band_select(&band, 240000000u));
    assert(requested == 1u && selected == 2u);
    assert(h9_main_clock_band_select(&band, 50000000u));
    assert(selected == 0u);
    assert(h9_main_clock_band_select(&band, 250000000u));
    assert(selected == 3u);
    assert(h9_main_clock_band_select(&band, 350000000u));
    assert(selected == 3u);
    enabled = 0u;
    requested = 0u;
    assert(h9_main_clock_band_select(&band, 50000000u));
    assert(requested == 1u && selected == 3u);
    assert(storage[0] == 0xA5A5A5A5u &&
           storage[21] == 0xA5A5A5A5u);
}

static void test_main_clock_calculation(void)
{
    uint32_t storage[22];
    uint32_t *records = storage + 1u;
    uint8_t band = 2u;
    H9MainClockCalculation calculation = { records, &band };
    unsigned index;
    for (index = 0u; index < 22u; ++index)
        storage[index] = 0xA5A5A5A5u;
    records[10] = 600000000u;
    records[11] = 500000000u;
    records[13] = 550000000u;
    assert(h9_main_clock_calculate(&calculation, 240000000u) == 480000000u);
    records[13] = 200000000u;
    assert(h9_main_clock_calculate(&calculation, 240000000u) == 240000000u);
    records[13] = 700000000u;
    assert(h9_main_clock_calculate(&calculation, 240000000u) == 480000000u);
    records[10] = 239999999u;
    assert(h9_main_clock_calculate(&calculation, 240000000u) == UINT32_MAX);
    records[10] = 600000000u;
    records[11] = 239999999u;
    assert(h9_main_clock_calculate(&calculation, 240000000u) == UINT32_MAX);
    band = 4u;
    assert(h9_main_clock_calculate(&calculation, 240000000u) == UINT32_MAX);
    band = 2u;
    assert(h9_main_clock_calculate(&calculation, 0u) == UINT32_MAX);
    assert(storage[0] == 0xA5A5A5A5u &&
           storage[21] == 0xA5A5A5A5u);
}

static void test_main_clock_match(void)
{
    const uint32_t sources[4] = {
        192000000u, 137000000u, 320000000u, 480000000u
    };
    uint8_t primary[4] = { 1u, 2u, 4u, 8u };
    const uint8_t secondary[4] = { 1u, 3u, 5u, 7u };
    uint8_t source = 0xA5u;
    uint8_t divider = 0xA5u;
    uint32_t result = 0u;
    H9MainClockMatch match = {
        sources, primary, secondary, &source, &divider, &result
    };
    assert(!h9_main_clock_match(NULL, 240000000u));
    assert(h9_main_clock_match(&match, 240000000u));
    assert(source == 3u && divider == 4u && result == 240000000u);
    assert(h9_main_clock_match(&match, 480000000u));
    assert(source == 3u && divider == 0u && result == 480000000u);
    assert(h9_main_clock_match(&match, 192000000u));
    assert(source == 0u && divider == 0u && result == 192000000u);
    assert(h9_main_clock_match(&match, 137000000u));
    assert(source == 1u && divider == 0u && result == 137000000u);
    assert(!h9_main_clock_match(&match, 123456789u));
    assert(source == 1u && divider == 0u && result == UINT32_MAX);
    primary[1] = 0u;
    assert(!h9_main_clock_match(&match, 240000000u));
    assert(result == UINT32_MAX);
}

static void test_main_clock_timing(void)
{
    uint32_t records[20] = { 0 };
    uint8_t band = 2u;
    uint32_t stage = 0xA5A5A5A5u;
    uint32_t timebase = 0xA5A5A5A5u;
    uint8_t first = 0xA5u;
    uint8_t third = 0xA5u;
    uint8_t rate = 0xA5u;
    uint16_t fourth = 0xA5A5u;
    uint8_t fifth = 0xA5u;
    uint32_t intermediate = 0xA5A5A5A5u;
    H9MainClockTiming timing = {
        records, &band, &stage, &timebase, &first, &third, &rate,
        &fourth, &fifth, &intermediate
    };
    records[10] = 200000000u;
    records[11] = 100000000u;
    records[12] = 80000000u;
    records[13] = 50000000u;
    assert(!h9_main_clock_timing_calculate(NULL, 480000000u, 240000000u));
    assert(!h9_main_clock_timing_calculate(&timing, 480000000u, 0u));
    assert(first == 0xA5u && intermediate == 0xA5A5A5A5u);
    assert(h9_main_clock_timing_calculate(&timing, 480000000u,
                                          240000000u));
    assert(first == 2u && rate == 4u && intermediate == 160000000u);
    assert(third == 3u && fourth == 1u && stage == 80000000u);
    assert(fifth == 0u && timebase == 80000000u);

    records[13] = 0u;
    assert(!h9_main_clock_timing_calculate(&timing, 480000000u,
                                           240000000u));
    assert(first == 2u && third == 3u && stage == 80000000u);
    records[10] = 500000000u;
    records[11] = 500000000u;
    records[12] = 500000000u;
    records[13] = 500000000u;
    assert(h9_main_clock_timing_calculate(&timing, 480000000u,
                                          40000000u));
    assert(first == 0u && fourth == 11u && stage == 40000000u);
    assert(fifth == 0u && timebase == 40000000u);
}

typedef struct {
    volatile uint32_t *control;
    unsigned calls;
} ClockApplyCapture;

static void clock_apply_rom(void *context)
{
    ClockApplyCapture *capture = (ClockApplyCapture *)context;
    assert(capture->calls == 0u && *capture->control == 0x1CCu);
    capture->calls++;
}

static void clock_apply_sync(void *context)
{
    ClockApplyCapture *capture = (ClockApplyCapture *)context;
    assert(capture->calls == 1u && *capture->control == 0x1CCu);
    capture->calls++;
}

static void clock_apply_delay(void *context, uint32_t count)
{
    ClockApplyCapture *capture = (ClockApplyCapture *)context;
    assert(capture->calls == 2u && count == 100u &&
           *capture->control == 0x1C2u);
    capture->calls++;
}

static void test_main_clock_apply_first(void)
{
    uint32_t control = 0x1CCu;
    ClockApplyCapture capture = { &control, 0u };
    H9MainClockApply apply = {
        &control, &capture, clock_apply_rom,
        clock_apply_sync, clock_apply_delay
    };
    assert(!h9_main_clock_apply_first(NULL));
    apply.synchronize = NULL;
    assert(!h9_main_clock_apply_first(&apply));
    assert(control == 0x1CCu && capture.calls == 0u);
    apply.synchronize = clock_apply_sync;
    assert(h9_main_clock_apply_first(&apply));
    assert(capture.calls == 3u && control == 2u);
}

typedef struct {
    uint32_t group[2];
    uint32_t field[2];
    uint32_t value[2];
    unsigned calls;
} ClockRomModeCapture;

static void clock_rom_mode_call(void *context, uint32_t group, uint32_t zero,
                                uint32_t field, uint32_t value)
{
    ClockRomModeCapture *capture = (ClockRomModeCapture *)context;
    assert(capture->calls < 2u && zero == 0u);
    capture->group[capture->calls] = group;
    capture->field[capture->calls] = field;
    capture->value[capture->calls] = value;
    capture->calls++;
}

static void test_main_clock_rom_mode(void)
{
    static const uint8_t expected[4] = { 7u, 8u, 10u, 14u };
    uint8_t enabled = 1u;
    uint8_t band = 0u;
    ClockRomModeCapture capture = { { 0 }, { 0 }, { 0 }, 0u };
    H9MainClockRomMode mode = {
        &enabled, &band, &capture, clock_rom_mode_call
    };
    unsigned index;
    assert(!h9_main_clock_rom_mode_apply(NULL));
    for (index = 0u; index < 4u; ++index) {
        capture.calls = 0u;
        band = (uint8_t)index;
        assert(h9_main_clock_rom_mode_apply(&mode));
        assert(capture.calls == 2u && capture.group[0] == 9u &&
               capture.field[0] == 4u &&
               capture.value[0] == expected[index]);
        assert(capture.group[1] == 6u && capture.field[1] == 3u &&
               capture.value[1] == 7u);
    }
    band = 4u;
    capture.calls = 0u;
    assert(!h9_main_clock_rom_mode_apply(&mode));
    assert(capture.calls == 0u);
    enabled = 0u;
    assert(h9_main_clock_rom_mode_apply(&mode));
    assert(capture.calls == 2u && capture.value[0] == 0u &&
           capture.value[1] == 0u);
}

static void test_main_clock_source_registers(void)
{
    uint32_t storage[15];
    uint32_t *control = storage + 1u;
    uint8_t source = 3u;
    uint8_t divider = 4u;
    uint32_t result = 240000000u;
    H9MainClockRegisters registers = {
        control, &source, &divider, &result
    };
    unsigned index;
    for (index = 0u; index < 15u; ++index)
        storage[index] = 0xA5A5A5A5u;
    assert(!h9_main_clock_program_source(NULL));
    assert(h9_main_clock_program_source(&registers));
    assert(control[0] == ((0xA5A5A5A5u & ~0x3Fu) | 0x13u));
    assert(control[12] == 0xA5A5A5A5u);
    result = UINT32_MAX;
    source = 0u;
    divider = 0u;
    assert(!h9_main_clock_program_source(&registers));
    assert(control[0] == ((0xA5A5A5A5u & ~0x3Fu) | 0x13u));
    assert(h9_main_clock_latch_source(&registers));
    assert(control[12] == (0xA5A5A5A5u | 0x1F000000u));
    assert(storage[0] == 0xA5A5A5A5u &&
           storage[14] == 0xA5A5A5A5u);
}

typedef struct {
    volatile uint32_t *control;
    uint32_t first;
    uint32_t second;
    unsigned calls;
} ClockAuxRomCall;

static void clock_aux_rom_pair(void *context, uint32_t first,
                               uint32_t second)
{
    ClockAuxRomCall *call = (ClockAuxRomCall *)context;
    assert(*call->control == ((0xA5A5A5A5u & ~0x7000u) | 0x6000u));
    call->first = first;
    call->second = second;
    call->calls++;
}

static void test_main_clock_aux_fields(void)
{
    uint32_t control = 0xA5A5A5A5u;
    uint8_t first = 0x12u;
    uint8_t second = 0x34u;
    uint8_t high = 0xFEu;
    uint16_t low = 0x1234u;
    uint8_t middle = 0xFDu;
    ClockAuxRomCall call = { &control, 0u, 0u, 0u };
    H9MainClockAuxFields fields = {
        &control, &high, &second, &first, &low, &middle,
        &call, clock_aux_rom_pair
    };
    assert(!h9_main_clock_aux_program(NULL));
    fields.rom_apply_pair = NULL;
    assert(!h9_main_clock_aux_program(&fields));
    assert(control == 0xA5A5A5A5u);
    fields.rom_apply_pair = clock_aux_rom_pair;
    assert(h9_main_clock_aux_program(&fields));
    assert(call.calls == 1u && call.first == 0x12u && call.second == 0x34u);
    assert(control == ((0xA5A5A5A5u & ~0x77FFu) |
                       0x6000u | 0x500u | 0x34u));
}

typedef struct {
    volatile uint32_t *control;
    volatile uint32_t *aux_control;
    unsigned calls;
} ClockFinishCapture;

static void clock_finish_delay(void *context, uint32_t count)
{
    ClockFinishCapture *capture = (ClockFinishCapture *)context;
    assert(capture->control[2] == 0x53u);
    assert(capture->control[14] == 0x1FB03026u);
    if (capture->calls == 0u) {
        assert(count == 10u && capture->control[13] == 0xD57CFE01u);
    } else {
        assert(capture->calls == 1u && count == 20u &&
               capture->control[13] == 0xD57CFE03u);
    }
    capture->calls++;
}

static void clock_finish_aux_rom(void *context, uint32_t first,
                                 uint32_t second)
{
    ClockFinishCapture *capture = (ClockFinishCapture *)context;
    assert(capture->calls == 2u && first == 3u && second == 2u);
    assert(*capture->aux_control ==
           ((0xFFFFFFFFu & ~0x7000u) | 0x1000u));
    capture->calls++;
}

static void clock_finish_rom(void *context)
{
    ClockFinishCapture *capture = (ClockFinishCapture *)context;
    assert(capture->calls == 3u && capture->control[0] == 0x1C0u);
    assert((*capture->aux_control & 0x77FFu) == 0x1534u);
    capture->calls++;
}

static void test_main_clock_finish(void)
{
    uint32_t storage[16] = { 0 };
    uint32_t *control = storage + 1u;
    uint8_t source = 3u, divider = 4u;
    uint32_t result = 240000000u;
    uint8_t e8 = 1u, f0 = 2u, ec = 3u, f8 = 5u;
    uint16_t f4 = 0x1234u;
    ClockFinishCapture capture = { control, storage, 0u };
    H9MainClockRegisters registers = {
        control + 2u, &source, &divider, &result
    };
    H9MainClockAuxFields aux = {
        storage, &e8, &f0, &ec, &f4, &f8,
        &capture, clock_finish_aux_rom
    };
    H9MainClockFinish finish = {
        control, &registers, &aux, &capture,
        clock_finish_delay, clock_finish_rom
    };
    storage[0] = 0xFFFFFFFFu;
    control[0] = 0u;
    control[1] = 0xFFFFFFFFu;
    control[2] = 0xFFu;
    control[13] = 0xFFFFFFFFu;
    control[14] = 0u;
    assert(!h9_main_clock_finish_run(NULL));
    aux.rom_apply_pair = NULL;
    assert(!h9_main_clock_finish_run(&finish));
    assert(control[13] == 0xFFFFFFFFu && capture.calls == 0u);
    aux.rom_apply_pair = clock_finish_aux_rom;
    assert(h9_main_clock_finish_run(&finish));
    assert(capture.calls == 4u);
    assert(control[0] == 0x1C0u && control[1] == 0xFFFFF3F3u);
    assert(control[2] == 0x53u && control[13] == 0xD57CFE03u &&
           control[14] == 0x1FB03026u);
    assert((*storage & 0x77FFu) == 0x1534u);
}

typedef struct {
    uint32_t first;
    uint32_t second;
    unsigned reads;
    unsigned sets;
} PostClockCapture;

static uint32_t post_clock_read(void *context, uint32_t selector)
{
    PostClockCapture *capture = (PostClockCapture *)context;
    assert(selector == (capture->reads == 0u ? 49u : 51u));
    assert(capture->reads < 2u);
    capture->reads++;
    return selector == 49u ? capture->first : capture->second;
}

static void post_clock_set(void *context, uint32_t group, uint32_t value)
{
    PostClockCapture *capture = (PostClockCapture *)context;
    assert(capture->reads == 2u && capture->sets == 0u);
    assert(group == 17u && value == 1u);
    capture->sets++;
}

static void test_main_post_clock_rom(void)
{
    PostClockCapture capture = { 0x80u, 0x80u, 0u, 0u };
    H9MainPostClockRom rom = { &capture, post_clock_read, post_clock_set };
    H9MainPostClockWords words = { 0u, 0u };
    assert(!h9_main_post_clock_rom_run(NULL, &words));
    assert(!h9_main_post_clock_rom_run(&rom, NULL));
    rom.set_control = NULL;
    assert(!h9_main_post_clock_rom_run(&rom, &words));
    assert(capture.reads == 0u && words.first_word == 0u);
    rom.set_control = post_clock_set;

    /* Signed low byte 0x80 skips selector 51 even if upper bits are clear. */
    assert(h9_main_post_clock_rom_run(&rom, &words));
    assert(capture.reads == 1u && capture.sets == 0u);
    assert(words.first_word == UINT32_MAX &&
           words.second_word == UINT32_MAX);

    capture.first = 0x100u;
    capture.second = 0x7Fu;
    capture.reads = 0u;
    assert(h9_main_post_clock_rom_run(&rom, &words));
    assert(capture.reads == 2u && capture.sets == 0u);

    capture.second = 0x80u;
    capture.reads = 0u;
    assert(h9_main_post_clock_rom_run(&rom, &words));
    assert(capture.reads == 2u && capture.sets == 1u);
}

typedef struct {
    uint32_t response;
    unsigned reads;
} PostClockClassCapture;

static uint32_t post_clock_class_read(void *context, uint32_t selector)
{
    PostClockClassCapture *capture = (PostClockClassCapture *)context;
    assert(selector == 148u);
    capture->reads++;
    return capture->response;
}

static void test_main_post_clock_classification(void)
{
    uint32_t flags = 1u, channel = 3u << 21u, strap = 0u;
    uint32_t secondary = 0xA55A1234u;
    uint8_t mode = 0xEEu;
    PostClockClassCapture capture = { 0x123456FEu, 0u };
    H9MainPostClockRom rom = { &capture, post_clock_class_read, NULL };
    H9MainPostClockClassifier classifier = {
        &flags, &channel, &strap, &secondary, &mode, &rom
    };
    H9MainPostClockClassification result;
    unsigned index;
    assert(!h9_main_post_clock_classify(NULL, &result));
    assert(!h9_main_post_clock_classify(&classifier, NULL));
    rom.read_selector = NULL;
    assert(!h9_main_post_clock_classify(&classifier, &result));
    assert(capture.reads == 0u && mode == 0xEEu);
    rom.read_selector = post_clock_class_read;

    for (index = 0u; index < 16u; ++index) {
        uint8_t expected = (index >= 2u && index <= 9u) ?
            ((index & 1u) == 0u ? 2u : 4u) : 1u;
        strap = index << 8u;
        mode = 0xEEu;
        capture.reads = 0u;
        assert(h9_main_post_clock_classify(&classifier, &result));
        assert(capture.reads == 1u && result.strap_index == index);
        assert(mode == expected && result.mode == expected);
        assert(result.channel_flag == 0u);
        assert(result.selector_148_masked == 0x12345600u);
        assert(result.secondary_status == secondary);
    }
    channel = 1u << 21u;
    strap = 2u << 8u;
    assert(h9_main_post_clock_classify(&classifier, &result));
    assert(result.channel_flag == 1u);
    flags = 0u;
    assert(h9_main_post_clock_classify(&classifier, &result));
    assert(result.channel_flag == 0u);
}

typedef struct {
    uint32_t response;
    unsigned reads;
} PostClockBitmapCapture;

static uint32_t post_clock_bitmap_read(void *context, uint32_t selector)
{
    PostClockBitmapCapture *capture = (PostClockBitmapCapture *)context;
    assert(selector == 192u);
    capture->reads++;
    return capture->response;
}

static void test_main_post_clock_bitmap(void)
{
    PostClockBitmapCapture capture = { 0u, 0u };
    H9MainPostClockRom rom = { &capture, post_clock_bitmap_read, NULL };
    H9MainPostClockWords words = { UINT32_MAX, UINT32_MAX };
    H9MainPostClockClassification classification = { 0 };
    H9MainPostClockRouteFlags flags = { false, false };
    unsigned index;
    assert(h9_main_post_clock_bitmap_step(NULL, &words, &classification,
                                           &flags) ==
           H9_MAIN_POST_CLOCK_BITMAP_ERROR);
    assert(h9_main_post_clock_bitmap_step(&rom, NULL, &classification,
                                           &flags) ==
           H9_MAIN_POST_CLOCK_BITMAP_ERROR);
    assert(h9_main_post_clock_bitmap_step(&rom, &words, NULL, &flags) ==
           H9_MAIN_POST_CLOCK_BITMAP_ERROR);
    assert(h9_main_post_clock_bitmap_step(&rom, &words, &classification,
                                           NULL) ==
           H9_MAIN_POST_CLOCK_BITMAP_ERROR);
    classification.mode = 1u;
    assert(h9_main_post_clock_bitmap_step(&rom, &words, &classification,
                                           &flags) ==
           H9_MAIN_POST_CLOCK_BITMAP_CONTINUE);
    assert(capture.reads == 0u && words.first_word == UINT32_MAX &&
           words.second_word == UINT32_MAX && !flags.r6_flag &&
           !flags.r9_flag);

    classification.selector_148_masked = 1u;
    for (index = 0u; index <= 55u; ++index) {
        capture.response = index;
        capture.reads = 0u;
        words.first_word = UINT32_MAX;
        words.second_word = UINT32_MAX;
        assert(h9_main_post_clock_bitmap_step(&rom, &words,
                                               &classification, &flags) ==
               H9_MAIN_POST_CLOCK_BITMAP_CONTINUE);
        assert(capture.reads == 1u);
        assert(words.first_word ==
               (index < 32u ? UINT32_MAX & ~(1u << index) : UINT32_MAX));
        assert(words.second_word ==
               (index >= 32u ? UINT32_MAX & ~(1u << (index - 32u)) : UINT32_MAX));
    }
    for (index = 56u; index <= 58u; ++index) {
        capture.response = index;
        capture.reads = 0u;
        words.first_word = UINT32_MAX;
        words.second_word = UINT32_MAX;
        assert(h9_main_post_clock_bitmap_step(&rom, &words,
                                               &classification, &flags) ==
               H9_MAIN_POST_CLOCK_BITMAP_NO_CLEAR);
        assert(capture.reads == 1u && words.first_word == UINT32_MAX &&
               words.second_word == UINT32_MAX);
        assert(flags.r6_flag == (index == 56u));
        assert(flags.r9_flag == (index == 57u));
    }
    classification.channel_flag = 1u;
    capture.response = 58u;
    assert(h9_main_post_clock_bitmap_step(&rom, &words, &classification,
                                           &flags) ==
           H9_MAIN_POST_CLOCK_BITMAP_NO_CLEAR);
    assert(flags.r6_flag && flags.r9_flag);
}

typedef struct {
    uint32_t mask;
    uint32_t transformed;
    unsigned calls;
} PostClockHandoffCapture;

static uint32_t post_clock_transform_mask(void *context, uint32_t mask)
{
    PostClockHandoffCapture *capture = (PostClockHandoffCapture *)context;
    assert(capture->calls % 2u == 0u);
    capture->mask = mask;
    capture->transformed = mask ^ 0xA5A5A5A5u;
    ++capture->calls;
    return capture->transformed;
}

static uint64_t post_clock_query_pair(void *context, uint32_t r0,
                                      uint32_t r1)
{
    PostClockHandoffCapture *capture = (PostClockHandoffCapture *)context;
    assert(capture->calls % 2u == 1u);
    assert(r0 == capture->transformed && r1 == r0);
    ++capture->calls;
    return UINT64_C(0x89ABCDEF12345678);
}

static void test_main_post_clock_handoff(void)
{
    PostClockHandoffCapture capture = { 0u, 0u, 0u };
    H9MainPostClockHandoff handoff = {
        &capture, post_clock_transform_mask, post_clock_query_pair
    };
    H9MainPostClockHandoffResult result;
    assert(!h9_main_post_clock_handoff_run(NULL, 0u, &result));
    assert(!h9_main_post_clock_handoff_run(&handoff, 0u, NULL));
    assert(h9_main_post_clock_handoff_run(&handoff, 0u, &result));
    assert(capture.calls == 2u && capture.mask == UINT32_MAX);
    assert(result.input_mask == UINT32_MAX &&
           result.transformed_mask == capture.transformed &&
           result.result_r0 == 0x12345678u &&
           result.result_r1 == 0x89ABCDEFu);
    assert(h9_main_post_clock_handoff_run(&handoff, 1u, &result));
    assert(capture.calls == 4u && capture.mask == 0xFFFFFE3Bu);
    assert(result.input_mask == 0xFFFFFE3Bu);
}

typedef struct {
    volatile uint32_t descriptor0[9];
    volatile uint32_t descriptor16[9];
    volatile uint32_t descriptor32[9];
    volatile uint32_t descriptor48[9];
    volatile uint32_t descriptor15[6];
    volatile uint32_t descriptor23[6];
    volatile uint32_t descriptor53[6];
    uint32_t queried[4];
    unsigned queries;
    bool omit53;
    bool omit32;
} PostClockDescriptorCapture;

static volatile uint32_t *post_clock_lookup_descriptor(void *context,
                                                        uint32_t index)
{
    PostClockDescriptorCapture *capture =
        (PostClockDescriptorCapture *)context;
    assert(capture->queries < 4u);
    capture->queried[capture->queries++] = index;
    if (index == 0u) return capture->descriptor0;
    if (index == 16u) return capture->descriptor16;
    if (index == 32u && !capture->omit32) return capture->descriptor32;
    if (index == 48u) return capture->descriptor48;
    if (index == 15u) return capture->descriptor15;
    if (index == 23u) return capture->descriptor23;
    if (index == 53u && !capture->omit53) return capture->descriptor53;
    return NULL;
}

static void test_main_post_clock_descriptor_route(void)
{
    PostClockDescriptorCapture capture = { 0 };
    H9MainPostClockDescriptorRoute route = {
        &capture, post_clock_lookup_descriptor
    };
    H9MainPostClockDescriptorRoute target;
    H9MainPostClockHandoffResult handoff = { 0 };
    H9MainPostClockDescriptorResult result;
    unsigned i;
    h9_main_post_clock_descriptor_route_bind_target(&target);
    assert((uintptr_t)target.lookup_descriptor(NULL, 15u) == 0x1E5000u);
    assert((uintptr_t)target.lookup_descriptor(NULL, 23u) == 0x1E5040u);
    assert((uintptr_t)target.lookup_descriptor(NULL, 53u) == 0x1E50C0u);
    assert((uintptr_t)target.lookup_descriptor(NULL, 57u) == 0u);
    assert(!h9_main_post_clock_descriptor_route_run(NULL, &handoff, &result));
    assert(!h9_main_post_clock_descriptor_route_run(&route, NULL, &result));
    handoff.input_mask = 0xFFFFFE3Bu;
    handoff.result_r0 = 4u;
    handoff.result_r1 = 1u;
    assert(h9_main_post_clock_descriptor_route_run(&route, &handoff, &result));
    assert(capture.queries == 0u && result.filtered_mask == 0x1E3Bu &&
           result.base_word == 0xFFFF0000u &&
           result.control_word == 0xFFFF0033u);
    handoff.result_r1 = 0u;
    assert(h9_main_post_clock_descriptor_route_run(&route, &handoff, &result));
    assert(capture.queries == 0u &&
           result.filtered_mask == handoff.input_mask &&
           result.base_word == 0xFFFF0080u &&
           result.control_word == 0xFFFF003Fu);
    for (i = 0u; i < 6u; ++i) {
        capture.descriptor15[i] = UINT32_MAX;
        capture.descriptor23[i] = UINT32_MAX;
        capture.descriptor53[i] = UINT32_MAX;
    }
    handoff.result_r0 = 3u;
    handoff.result_r1 = 1u;
    assert(h9_main_post_clock_descriptor_route_run(&route, &handoff, &result));
    assert(capture.queries == 2u && capture.queried[0] == 15u &&
           capture.queried[1] == 53u);
    assert(capture.descriptor15[4] == (UINT32_MAX & ~0x8000u) &&
           capture.descriptor15[5] == (UINT32_MAX & ~0x8000u));
    assert(capture.descriptor53[4] == (UINT32_MAX & ~0x20u) &&
           capture.descriptor53[5] == (UINT32_MAX & ~0x20u));
    assert(result.filtered_mask == (handoff.input_mask & 0x9FFFu) &&
           result.base_word == 0xFFFF0000u &&
           result.control_word == 0xFFFF0013u);
    capture.queries = 0u;
    capture.omit53 = true;
    handoff.result_r1 = 0u;
    assert(h9_main_post_clock_descriptor_route_run(&route, &handoff, &result));
    assert(capture.queries == 2u && capture.queried[0] == 23u &&
           capture.queried[1] == 53u);
    assert(capture.descriptor23[4] == (UINT32_MAX & ~0x80u) &&
           capture.descriptor23[5] == (UINT32_MAX & ~0x80u));
    assert(result.filtered_mask == handoff.input_mask &&
           result.control_word == 0xFFFF001Fu);
    {
        H9MainPostClockWords words = { 0xABCD1234u, 0xFFFFFFFFu };
        H9MainPostClockDescriptorResult profile = {
            0xFFFFFE3Bu, 0xFFFF0080u, 0xFFFF003Fu
        };
        capture.queries = 0u;
        for (i = 0u; i < 9u; ++i) {
            capture.descriptor0[i] = UINT32_MAX;
            capture.descriptor16[i] = UINT32_MAX;
        }
        assert(!h9_main_post_clock_apply_initial_masks(NULL, &words,
                                                        &profile));
        assert(h9_main_post_clock_apply_initial_masks(&route, &words,
                                                       &profile));
        assert(capture.queries == 4u && capture.queried[0] == 0u &&
               capture.queried[1] == 0u && capture.queried[2] == 16u &&
               capture.queried[3] == 16u);
        assert(capture.descriptor0[5] == 0xFFFF01C4u);
        assert(capture.descriptor0[8] == 0xFFFFEDCBu);
        assert(capture.descriptor16[5] == 0xFFFF0080u);
        assert(capture.descriptor16[8] == 0xFFFF5432u);
        assert(capture.descriptor0[4] == UINT32_MAX &&
               capture.descriptor16[4] == UINT32_MAX);
    }
    {
        H9MainPostClockWords words = { UINT32_MAX, 0xABCD1234u };
        H9MainPostClockDescriptorResult profile = {
            UINT32_MAX, UINT32_MAX, 0xFFFF0013u
        };
        H9MainPostClockDescriptor32Board board;
        uint32_t bank_status = 2u;
        uint32_t guard = 0u;
        uint32_t control_regs[13];
        uint8_t state_byte = 1u;
        h9_main_post_clock_descriptor32_bind_target(&board);
        assert((uintptr_t)board.bank_status_1e0200 == 0x1E0200u &&
               (uintptr_t)board.expected_descriptor32_1e5080 == 0x1E5080u &&
               (uintptr_t)board.state_byte_d975 == 0xD975u &&
               (uintptr_t)board.state_guard_d90c == 0xD90Cu &&
               (uintptr_t)board.control_registers_1e19c0 == 0x1E19C0u);
        board.bank_status_1e0200 = &bank_status;
        board.expected_descriptor32_1e5080 = capture.descriptor32;
        board.state_byte_d975 = &state_byte;
        board.state_guard_d90c = &guard;
        board.control_registers_1e19c0 = control_regs;
        for (i = 0u; i < 9u; ++i) {
            capture.descriptor32[i] = UINT32_MAX;
            capture.descriptor48[i] = UINT32_MAX;
        }
        for (i = 0u; i < 13u; ++i) control_regs[i] = UINT32_MAX;
        capture.descriptor32[3] = 0xDEADBEEFu;
        capture.queries = 0u;
        assert(!h9_main_post_clock_descriptor32_run(NULL, &board,
                                                       &words, &profile));
        assert(h9_main_post_clock_descriptor32_run(&route, &board,
                                                    &words, &profile));
        assert(capture.queries == 4u && capture.queried[0] == 32u &&
               capture.queried[1] == 32u && capture.queried[2] == 48u &&
               capture.queried[3] == 48u);
        assert(capture.descriptor32[0] == 0xFFFFFFBFu &&
               capture.descriptor32[2] == 0xFFFFFFBFu &&
               capture.descriptor32[3] == 0xDEAD0040u &&
               capture.descriptor32[4] == 0xFFFFFFBFu &&
               capture.descriptor32[8] == 0xFFFFEDCBu);
        assert(capture.descriptor48[5] == 0xFFFF0013u &&
               capture.descriptor48[8] == 0xFFFF5432u);
        assert(state_byte == 0u && control_regs[0] == 0xFFFFFFFCu &&
               control_regs[3] == 0xFFFEFFFFu &&
               control_regs[8] == 0xFFFCFFFFu &&
               control_regs[10] == 0xFFFFBFFFu &&
               control_regs[12] == 0xFFEEFFFFu);
        bank_status = 3u;
        state_byte = 1u;
        capture.queries = 0u;
        capture.descriptor32[3] = 0x12345678u;
        assert(h9_main_post_clock_descriptor32_run(&route, &board,
                                                    &words, &profile));
        assert(state_byte == 1u &&
               capture.descriptor32[3] == 0x12340040u);
        bank_status = 0u;
        guard = 1u;
        state_byte = 1u;
        capture.queries = 0u;
        for (i = 0u; i < 13u; ++i) control_regs[i] = UINT32_MAX;
        assert(h9_main_post_clock_descriptor32_run(&route, &board,
                                                    &words, &profile));
        assert(state_byte == 0u && control_regs[0] == UINT32_MAX &&
               control_regs[3] == UINT32_MAX &&
               control_regs[8] == 0xFFFCFFFFu);
        capture.omit32 = true;
        capture.queries = 0u;
        state_byte = 1u;
        for (i = 0u; i < 9u; ++i) capture.descriptor48[i] = UINT32_MAX;
        assert(h9_main_post_clock_descriptor32_run(&route, &board,
                                                    &words, &profile));
        assert(capture.queries == 4u && state_byte == 1u &&
               capture.descriptor48[5] == 0xFFFF0013u &&
               capture.descriptor48[8] == 0xFFFF5432u);
    }
}

static void test_main_post_clock_gate(void)
{
    H9MainPostClockGateRegisters registers;
    H9MainPostClockRouteFlags flags = { false, false };
    uint32_t control[2] = { 0x12345678u, 0x87654321u };
    uint32_t state = UINT32_MAX;
    h9_main_post_clock_gate_bind_target(&registers);
    assert((uintptr_t)registers.control_1e1800 == 0x1E1800u &&
           (uintptr_t)registers.state_1e5100 == 0x1E5100u);
    registers.control_1e1800 = control;
    registers.state_1e5100 = &state;
    assert(!h9_main_post_clock_gate_run(NULL, &flags));
    assert(!h9_main_post_clock_gate_run(&registers, NULL));
    assert(h9_main_post_clock_gate_run(&registers, &flags));
    assert(control[0] == 0u && control[1] == 0u &&
           state == 0xFFFF990Fu);
    control[0] = 0x12345678u;
    control[1] = 0x87654321u;
    state = UINT32_MAX;
    flags.r9_flag = true;
    assert(h9_main_post_clock_gate_run(&registers, &flags));
    assert(control[0] == 0u && control[1] == 0u &&
           state == 0xFFFFDDAFu);
    control[0] = 0x12345678u;
    control[1] = 0x87654321u;
    state = UINT32_MAX;
    flags.r6_flag = true;
    flags.r9_flag = false;
    assert(h9_main_post_clock_gate_run(&registers, &flags));
    assert(control[0] == 0u && control[1] == 0u &&
           state == 0xFFFFBB5Fu);
    control[0] = 0x12345678u;
    control[1] = 0x87654321u;
    state = UINT32_MAX;
    flags.r9_flag = true;
    assert(h9_main_post_clock_gate_run(&registers, &flags));
    assert(control[0] == 0x12345678u && control[1] == 0x87654321u &&
           state == UINT32_MAX);
}

typedef struct {
    unsigned events;
    uint32_t first;
    uint32_t second;
} PostClockTimerCapture;

static void post_clock_timer_handler(void) { }

static void post_clock_timer_setup(void *context, uint32_t mode,
                                   uint32_t value)
{
    PostClockTimerCapture *capture = (PostClockTimerCapture *)context;
    assert(capture->events == 0u && mode == 0x809Bu && value == 0u);
    capture->events = 1u;
}

static void post_clock_timer_install(void *context, uint32_t irq,
                                     uint32_t priority, void (*handler)(void))
{
    PostClockTimerCapture *capture = (PostClockTimerCapture *)context;
    assert(capture->events == 1u && irq == 3u && priority == 1u &&
           handler == post_clock_timer_handler);
    capture->events = 2u;
}

static uint32_t post_clock_timer_read(void *context, uint32_t selector)
{
    PostClockTimerCapture *capture = (PostClockTimerCapture *)context;
    assert(capture->events >= 2u);
    if (selector == 49u) {
        capture->events = 3u;
        return capture->first;
    }
    assert(selector == 18u && capture->events == 4u);
    capture->events = 5u;
    return capture->second;
}

static void post_clock_timer_control(void *context, uint32_t selector,
                                     uint32_t value)
{
    PostClockTimerCapture *capture = (PostClockTimerCapture *)context;
    assert(capture->events == 3u && selector == 49u && value == 64u);
    capture->events = 4u;
}

static void test_main_post_clock_timer_stage(void)
{
    PostClockTimerCapture capture = { 0u, 0x23u, 0xC2u };
    H9MainPostClockRom rom = {
        &capture, post_clock_timer_read, post_clock_timer_control
    };
    uint32_t source = 240000000u;
    uint32_t timer[3] = { 0u, 1u, 2u };
    uint32_t dummy = 0x12345678u;
    uint8_t output = 0u;
    H9MainPostClockTimerStage stage = {
        &capture, post_clock_timer_setup, post_clock_timer_install,
        post_clock_timer_handler, &source, timer, &dummy, &output, &rom
    };
    H9MainPostClockTimerStage target;
    h9_main_post_clock_timer_stage_bind_target(&target, &rom);
    assert((uintptr_t)target.clock_count_d9d8 == 0xD9D8u &&
           (uintptr_t)target.timer_registers_10f0ec == 0x10F0ECu &&
           (uintptr_t)target.dummy_read_1e00c0 == 0x1E00C0u &&
           (uintptr_t)target.selector_byte_11b40 == 0x11B40u &&
           target.timer_irq_handler == h9_main_timer3_isr);
    assert(!h9_main_post_clock_timer_stage_run(NULL));
    stage.timer_irq_handler = NULL;
    assert(!h9_main_post_clock_timer_stage_run(&stage));
    assert(capture.events == 0u && timer[0] == 0u);
    stage.timer_irq_handler = post_clock_timer_handler;
    assert(h9_main_post_clock_timer_stage_run(&stage));
    assert(capture.events == 3u && output == 0x23u);
    assert(timer[0] == 0x41u && timer[1] == 0u &&
           timer[2] == 2399999u);
    capture.events = 0u;
    capture.first = 0x180u;
    assert(h9_main_post_clock_timer_stage_run(&stage));
    assert(capture.events == 5u && output == 0xC2u);
}

typedef struct {
    unsigned enters;
    unsigned leaves;
    uint32_t bank;
    uint32_t priority;
    uint32_t shift;
    uint32_t field;
    uint32_t value;
} IrqRegistrationCapture;

static void irq_registration_enter(void *context, uint32_t bank,
                                   uint32_t priority, uint32_t shift)
{
    IrqRegistrationCapture *capture = (IrqRegistrationCapture *)context;
    ++capture->enters;
    capture->bank = bank;
    capture->priority = priority;
    capture->shift = shift;
}

static void irq_registration_leave(void *context, uint32_t value,
                                   uint32_t field, uint32_t shift)
{
    IrqRegistrationCapture *capture = (IrqRegistrationCapture *)context;
    ++capture->leaves;
    assert(capture->enters == capture->leaves &&
           capture->shift == shift);
    capture->value = value;
    capture->field = field;
}

typedef struct {
    unsigned reads;
} ClockHandshakeCapture;

static uint32_t clock_handshake_read(void *context,
                                     volatile uint32_t *status)
{
    ClockHandshakeCapture *capture = (ClockHandshakeCapture *)context;
    unsigned phase = capture->reads / 2u;
    unsigned attempt = capture->reads++ % 2u;
    unsigned desired = phase % 2u == 0u ? 1u : 0u;
    if (attempt == 1u) assert(*status == 0xE7u);
    return attempt == 0u ? desired ^ 1u : desired;
}

static uint32_t clock_handshake_stuck(void *context,
                                      volatile uint32_t *status)
{
    ClockHandshakeCapture *capture = (ClockHandshakeCapture *)context;
    (void)status;
    ++capture->reads;
    return 0u;
}

static uint32_t clock_handshake_ready(void *context,
                                      volatile uint32_t *status)
{
    ClockHandshakeCapture *capture = (ClockHandshakeCapture *)context;
    (void)status;
    ++capture->reads;
    return 1u;
}

static void test_main_clock_handshake(void)
{
    ClockHandshakeCapture capture = { 0u };
    uint32_t status = 0u, gates = 0x5u, control = 0x80u, other = 0x10u;
    uint32_t window[4] = { 0u, 0u, 0xCAFEu, 0xBABEu };
    H9MainClockHandshake handshake = {
        &capture, &status, &gates, &control, &other,
        clock_handshake_read, 2u
    };
    H9MainClockHandshake target;
    h9_main_clock_handshake_bind_target(&target, 100000u);
    assert((uintptr_t)target.status_101044 == 0x101044u &&
           (uintptr_t)target.gates_101000 == 0x101000u &&
           (uintptr_t)target.control_10f0d0 == 0x10F0D0u &&
           (uintptr_t)target.control_10f1c0 == 0x10F1C0u);
    assert(!h9_main_clock_handshake_run(NULL));
    handshake.poll_limit = 0u;
    assert(!h9_main_clock_handshake_run(&handshake));
    assert(capture.reads == 0u);
    handshake.poll_limit = 2u;
    assert(h9_main_clock_handshake_run(&handshake));
    assert(capture.reads == 40u && status == 0xE7u);
    assert(gates == 0x3F000005u && control == 0x87u && other == 0x11u);

    capture.reads = 0u;
    gates = 0x5u;
    control = 0x80u;
    other = 0x10u;
    handshake.read_status = clock_handshake_stuck;
    assert(!h9_main_clock_handshake_run(&handshake));
    assert(capture.reads == 2u && status == 0xE7u);
    assert(gates == 0x5u && control == 0x80u && other == 0x10u);

    handshake.window_words_101070 = window;
    assert((uintptr_t)target.window_words_101070 == 0x101070u);
    assert(!h9_main_clock_map_sources(&handshake, 0u, 0x1234u));
    capture.reads = 0u;
    handshake.read_status = clock_handshake_ready;
    status = 0x55u;
    assert(h9_main_clock_map_sources(&handshake, 0x1E00100u, 0x1EC628Cu));
    assert(capture.reads == 1u && status == 0x55u);
    assert(window[0] == 0x1EC628Cu && window[1] == 0x1E00100u &&
           window[2] == 0xCAFEu && window[3] == 0xBABEu);
    capture.reads = 0u;
    handshake.read_status = clock_handshake_stuck;
    window[0] = 0x1111u;
    window[1] = 0x2222u;
    assert(!h9_main_clock_map_sources(&handshake, 0x1E00100u, 0x1EC628Cu));
    assert(capture.reads == 2u && window[0] == 0x1111u &&
           window[1] == 0x2222u);
}

typedef struct {
    uint32_t prefix[16];
    uint32_t config;
    uint32_t words[4];
    unsigned fills;
    unsigned reads;
} PostMapCapture;

static bool post_map_fill(void *context)
{
    PostMapCapture *capture = (PostMapCapture *)context;
    unsigned index;
    ++capture->fills;
    for (index = 0u; index < 16u; ++index)
        capture->prefix[index] = UINT32_MAX;
    return true;
}

static bool post_map_fill_fail(void *context)
{
    PostMapCapture *capture = (PostMapCapture *)context;
    ++capture->fills;
    return false;
}

static uint32_t post_map_read(void *context, volatile uint32_t *status)
{
    PostMapCapture *capture = (PostMapCapture *)context;
    unsigned phase = capture->reads++;
    unsigned index;
    (void)status;
    assert(phase < 4u && capture->fills == 1u);
    for (index = 0u; index < 16u; ++index)
        assert(capture->prefix[index] == UINT32_MAX);
    if (phase == 0u) {
        assert(capture->config == 0x1001u &&
               capture->words[0] == 0u);
    } else if (phase < 3u) {
        assert(capture->config == 0x1C01u &&
               capture->words[0] == 0x66BFu && capture->words[1] == 0u);
    } else {
        assert(capture->config == 0x1F01u &&
               capture->words[2] == 0x66BFu && capture->words[3] == 0u);
    }
    return (phase & 1u) == 0u ? 1u : 0u;
}

static uint32_t post_map_stuck(void *context, volatile uint32_t *status)
{
    PostMapCapture *capture = (PostMapCapture *)context;
    (void)status;
    ++capture->reads;
    return 0u;
}

static void test_main_post_map_gate(void)
{
    PostMapCapture capture = { { 0u }, 0x1001u,
                               { 0u, 0x1234u, 0u, 0x5678u }, 0u, 0u };
    uint32_t status = 0u;
    H9MainClockHandshake handshake = {
        &capture, &status, NULL, NULL, NULL,
        post_map_read, 2u, NULL
    };
    H9MainPostMapGate gate = {
        &capture, &handshake, post_map_fill,
        &capture.config, capture.words
    };
    H9MainPostMapGate target;
    unsigned index;
    h9_main_post_map_gate_bind_target(&target, &handshake);
    assert((uintptr_t)target.config_101040 == 0x101040u &&
           (uintptr_t)target.words_101050 == 0x101050u &&
           target.handshake == &handshake);
    assert(!h9_main_post_map_gate_run(NULL));
    assert(h9_main_post_map_gate_run(&gate));
    assert(capture.fills == 1u && capture.reads == 4u);
    assert(capture.config == 0x1F01u &&
           capture.words[0] == 0x66BFu && capture.words[1] == 0u &&
           capture.words[2] == 0x66BFu && capture.words[3] == 0u);

    capture.fills = capture.reads = 0u;
    gate.fill_prefix_ones = post_map_fill_fail;
    assert(!h9_main_post_map_gate_run(&gate));
    assert(capture.fills == 1u && capture.reads == 0u);
    gate.fill_prefix_ones = post_map_fill;
    capture.fills = capture.reads = 0u;
    capture.config = 0x1001u;
    capture.words[0] = capture.words[2] = 0u;
    for (index = 0u; index < 16u; ++index) capture.prefix[index] = 0u;
    handshake.read_status = post_map_stuck;
    assert(!h9_main_post_map_gate_run(&gate));
    assert(capture.fills == 1u && capture.reads == 2u &&
           capture.config == 0x1001u &&
           capture.words[0] == 0u && capture.words[2] == 0u);
    for (index = 0u; index < 16u; ++index)
        assert(capture.prefix[index] == UINT32_MAX);
}

typedef struct {
    uint32_t words[5];
    unsigned reads;
    uint32_t mode;
} PostMapWindowCapture;

static uint32_t post_map_window_read(void *context,
                                     volatile uint32_t *status)
{
    PostMapWindowCapture *capture = (PostMapWindowCapture *)context;
    unsigned phase = capture->reads++;
    (void)status;
    assert(phase < 2u);
    if (phase == 0u) {
        assert(capture->words[2] == 0x2222u &&
               capture->words[3] == 0x3333u &&
               capture->words[4] == 0x4444u);
        return 1u;
    }
    assert(capture->words[2] ==
           (capture->mode == 0u ? 0xE290u : 0x2222u));
    assert(capture->words[3] == 0xE290u &&
           capture->words[4] == 0xDA90u);
    return 0u;
}

static uint32_t post_map_window_stuck(void *context,
                                      volatile uint32_t *status)
{
    PostMapWindowCapture *capture = (PostMapWindowCapture *)context;
    (void)status;
    ++capture->reads;
    return 0u;
}

static void test_main_post_map_window(void)
{
    PostMapWindowCapture capture = {
        { 0x0u, 0x1111u, 0x2222u, 0x3333u, 0x4444u }, 0u, 1u
    };
    uint32_t status = 0u;
    H9MainClockHandshake handshake = {
        &capture, &status, NULL, NULL, NULL,
        post_map_window_read, 2u, NULL
    };
    H9MainPostMapWindow window = { &handshake, capture.words };
    H9MainPostMapWindow target;
    h9_main_post_map_window_bind_target(&target, &handshake);
    assert((uintptr_t)target.words_10f0d8 == 0x10F0D8u &&
           target.handshake == &handshake);
    assert(!h9_main_post_map_window_run(NULL, 1u, 0xE290u, 0xDA90u));
    assert(h9_main_post_map_window_run(&window, 1u, 0xE290u, 0xDA90u));
    assert(capture.reads == 2u && capture.words[2] == 0x2222u &&
           capture.words[3] == 0xE290u && capture.words[4] == 0xDA90u);

    capture.reads = 0u;
    capture.mode = 0u;
    capture.words[2] = 0x2222u;
    capture.words[3] = 0x3333u;
    capture.words[4] = 0x4444u;
    assert(h9_main_post_map_window_run(&window, 0u, 0xE290u, 0xDA90u));
    assert(capture.words[2] == 0xE290u);

    capture.reads = 0u;
    capture.words[2] = 0x2222u;
    capture.words[3] = 0x3333u;
    capture.words[4] = 0x4444u;
    handshake.read_status = post_map_window_stuck;
    assert(!h9_main_post_map_window_run(&window, 1u, 0xE290u, 0xDA90u));
    assert(capture.reads == 2u && capture.words[2] == 0x2222u &&
           capture.words[3] == 0x3333u && capture.words[4] == 0x4444u);
}

static void test_main_irq_registration(void)
{
    IrqRegistrationCapture capture = { 0u };
    uint32_t vectors[64] = { 0u };
    uint32_t controls[8] = { 0u };
    H9MainIrqRegistration registration = {
        &capture, vectors, controls,
        irq_registration_enter, irq_registration_leave
    };
    H9MainIrqRegistration target;
    h9_main_irq_registration_bind_target(&target);
    assert((uintptr_t)target.vector_words_2bf00 == 0x2BF00u &&
           (uintptr_t)target.control_words_10f100 == 0x10F100u);
    assert(!h9_main_irq_register(NULL, 3u, 1u,
                                  post_clock_timer_handler));
    assert(!h9_main_irq_register(&registration, 64u, 1u,
                                  post_clock_timer_handler));
    assert(capture.enters == 0u && capture.leaves == 0u);
    controls[0] = UINT32_MAX;
    assert(h9_main_irq_register(&registration, 3u, 1u,
                                 post_clock_timer_handler));
    assert(vectors[3] == (uint32_t)(uintptr_t)post_clock_timer_handler &&
           controls[0] == 0xFFFF3FFFu && capture.bank == 0u &&
           capture.priority == 1u && capture.shift == 12u &&
           capture.field == 0x3000u && capture.value == controls[0]);
    assert(h9_main_irq_register(&registration, 0u, 2u,
                                 post_clock_timer_handler));
    assert((controls[0] & 0xFu) == 1u && capture.priority == 2u);
    assert(h9_main_irq_register(&registration, 1u, 2u,
                                 post_clock_timer_handler));
    assert((controls[0] & 0xF0u) == 0xF0u);
    assert(h9_main_irq_register(&registration, 63u, 2u,
                                 post_clock_timer_handler));
    assert(vectors[63] == (uint32_t)(uintptr_t)post_clock_timer_handler &&
           controls[7] == 0x10000000u && capture.bank == 7u &&
           capture.shift == 28u);
}

typedef struct {
    uint32_t poll_result;
    uint32_t check_result;
    char calls[8];
    unsigned length;
} Timer3IrqCapture;

static void timer3_record(Timer3IrqCapture *capture, char event)
{
    assert(capture->length < sizeof capture->calls);
    capture->calls[capture->length++] = event;
}

static uint32_t timer3_poll(void *context)
{
    Timer3IrqCapture *capture = (Timer3IrqCapture *)context;
    timer3_record(capture, 'P');
    return capture->poll_result;
}

static void timer3_enter(void *context)
{
    timer3_record((Timer3IrqCapture *)context, 'E');
}

static void timer3_sync(void *context)
{
    timer3_record((Timer3IrqCapture *)context, 'S');
}

static uint32_t timer3_check(void *context)
{
    Timer3IrqCapture *capture = (Timer3IrqCapture *)context;
    timer3_record(capture, 'C');
    return capture->check_result;
}

static void timer3_leave(void *context)
{
    timer3_record((Timer3IrqCapture *)context, 'L');
}

static void timer3_fault(void *context)
{
    timer3_record((Timer3IrqCapture *)context, 'F');
}

static void test_main_timer3_irq(void)
{
    Timer3IrqCapture capture = { 0u, 0u, { 0 }, 0u };
    uint32_t ticks = 2u, control = 0x80u, clocks = 9u;
    uint32_t expected = 0x1234u, actual = 0x1234u;
    H9MainTimer3Irq irq = {
        &capture, &ticks, &control, &clocks, &expected, &actual,
        timer3_poll, timer3_enter, timer3_sync, timer3_check,
        timer3_leave, timer3_fault
    };
    assert(!h9_main_timer3_irq_handle(NULL));
    irq.fault_swi0 = NULL;
    assert(!h9_main_timer3_irq_handle(&irq));
    assert(ticks == 2u && capture.length == 0u);
    irq.fault_swi0 = timer3_fault;
    assert(h9_main_timer3_irq_handle(&irq));
    assert(ticks == 3u && control == 0xC0u && clocks == 10u);
    assert(capture.length == 1u && memcmp(capture.calls, "P", 1u) == 0);

    capture.length = 0u;
    capture.poll_result = 1u;
    assert(h9_main_timer3_irq_handle(&irq));
    assert(capture.length == 5u &&
           memcmp(capture.calls, "PESCL", 5u) == 0);

    capture.length = 0u;
    capture.check_result = 1u;
    assert(h9_main_timer3_irq_handle(&irq));
    assert(capture.length == 5u &&
           memcmp(capture.calls, "PESCL", 5u) == 0);

    capture.length = 0u;
    actual = 0x5678u;
    assert(!h9_main_timer3_irq_handle(&irq));
    assert(capture.length == 5u &&
           memcmp(capture.calls, "PESCF", 5u) == 0);
}

static void test_main_exception_capture(void)
{
    uint32_t frame[H9_MAIN_EXCEPTION_FRAME_WORDS];
    unsigned index;
    h9_main_last_exception[0] = 0u;
    h9_main_exception_capture(NULL);
    assert(h9_main_last_exception[0] == 0u);
    for (index = 0; index < H9_MAIN_EXCEPTION_FRAME_WORDS; ++index)
        frame[index] = 0xABCD0000u + index;
    h9_main_exception_capture(frame);
    assert(h9_main_last_exception[0] == 0x48394654u);
    for (index = 0; index < H9_MAIN_EXCEPTION_FRAME_WORDS; ++index)
        assert(h9_main_last_exception[index + 1u] == frame[index]);
}

typedef struct {
    uint8_t bytes[192];
    size_t length;
    unsigned calls;
    bool fail;
} BootBannerCapture;

static bool boot_banner_capture(void *context, const uint8_t *data,
                                size_t length)
{
    BootBannerCapture *capture = (BootBannerCapture *)context;
    assert(length <= sizeof capture->bytes);
    memcpy(capture->bytes, data, length);
    capture->length = length;
    capture->calls++;
    return !capture->fail;
}

static void test_aprs_boot_banner(void)
{
    static const char expected[] =
        "888"
        "\r\n========== APRS Module Version Info ==========\r\n"
        "Software Version: H9R0.01\r\n"
        "Hardware Version: V1.0.0\r\n"
        "===========================================\r\n";
    BootBannerCapture capture = { { 0 }, 0u, 0u, false };
    assert(!h9_aprs_boot_banner_send(NULL, &capture));
    assert(capture.calls == 0u);
    assert(h9_aprs_boot_banner_send(boot_banner_capture, &capture));
    assert(capture.calls == 1u && capture.length == sizeof expected - 1u);
    assert(memcmp(capture.bytes, expected, capture.length) == 0);
    capture.fail = true;
    assert(!h9_aprs_boot_banner_send(boot_banner_capture, &capture));
}

static void test_aprs_uart2_maintenance(void)
{
    H9AprsUart2Maintenance state;
    unsigned i;
    h9_aprs_uart2_maintenance_init(NULL);
    assert(!h9_aprs_uart2_maintenance_due(NULL, 3000u, true, true));
    h9_aprs_uart2_maintenance_init(&state);
    for (i = 0u; i < 999u; ++i)
        assert(!h9_aprs_uart2_maintenance_due(&state, 2000u, true, true));
    assert(!h9_aprs_uart2_maintenance_due(&state, 2000u, true, true));
    assert(state.loop_passes == 0u && state.last_attempt_tick == 0u);
    for (i = 0u; i < 999u; ++i)
        assert(!h9_aprs_uart2_maintenance_due(&state, 2001u, true, true));
    assert(h9_aprs_uart2_maintenance_due(&state, 2001u, true, true));
    assert(state.last_attempt_tick == 2001u);
    for (i = 0u; i < 1000u; ++i)
        assert(!h9_aprs_uart2_maintenance_due(&state, 4001u, true, true));
    for (i = 0u; i < 999u; ++i)
        assert(!h9_aprs_uart2_maintenance_due(&state, 4002u, true, true));
    assert(h9_aprs_uart2_maintenance_due(&state, 4002u, true, true));
    for (i = 0u; i < 1000u; ++i)
        assert(!h9_aprs_uart2_maintenance_due(&state, 8000u, false, true));
    for (i = 0u; i < 1000u; ++i)
        assert(!h9_aprs_uart2_maintenance_due(&state, 9000u, true, false));
    for (i = 0u; i < 999u; ++i)
        assert(!h9_aprs_uart2_maintenance_due(&state, 10000u, true, true));
    assert(h9_aprs_uart2_maintenance_due(&state, 10000u, true, true));

    state.last_attempt_tick = UINT32_MAX - 1000u;
    for (i = 0u; i < 999u; ++i)
        assert(!h9_aprs_uart2_maintenance_due(&state, 1000u, true, true));
    assert(h9_aprs_uart2_maintenance_due(&state, 1000u, true, true));
}

typedef struct {
    unsigned calls;
    unsigned failures;
    uintptr_t entry;
    uintptr_t argument;
    uint8_t byte_4;
    uint16_t halfword_6;
    uint16_t halfword_8;
    const char *name;
    int result;
} TaskLaunchCapture;

static int capture_task_create(void *context, uintptr_t entry,
                               uintptr_t argument, uint8_t byte_4,
                               uint16_t halfword_6, uint16_t halfword_8,
                               const char *name)
{
    TaskLaunchCapture *capture = (TaskLaunchCapture *)context;
    ++capture->calls;
    capture->entry = entry;
    capture->argument = argument;
    capture->byte_4 = byte_4;
    capture->halfword_6 = halfword_6;
    capture->halfword_8 = halfword_8;
    capture->name = name;
    return capture->result;
}

static void capture_task_failure(void *context)
{
    ++((TaskLaunchCapture *)context)->failures;
}

static void test_main_task_registry(void)
{
    const H9MainTaskDescriptor *core = h9_main_task_find("app_core");
    const H9MainTaskDescriptor *app = h9_main_task_find("my_app");
    const H9MainTaskDescriptor *tone = h9_main_task_find("key_tone");
    TaskLaunchCapture capture = { 0 };
    H9MainTaskHooks hooks = {
        &capture, capture_task_create, capture_task_failure
    };
    H9MainTaskHooks target;
    char copied_core_name[] = "app_core";
    size_t index;
    assert(core == &h9_main_tasks[0] && core->byte_4 == 1u &&
           core->halfword_6 == 0x0380u && core->halfword_8 == 0x0300u);
    assert(app == &h9_main_tasks[1] && app->byte_4 == 2u &&
           app->halfword_6 == 0x0800u && app->halfword_8 == 0x0300u);
    assert(tone == &h9_main_tasks[25] && tone->halfword_8 == 0x0020u);
    assert(h9_main_task_find(NULL) == NULL);
    assert(h9_main_task_find("missing") == NULL);
    for (index = 0u; index < H9_MAIN_TASK_COUNT; ++index)
        assert(h9_main_task_find(h9_main_tasks[index].name) ==
               &h9_main_tasks[index]);
    assert(h9_main_task_launch(NULL, 1u, 0u, "app_core") ==
           H9_MAIN_TASK_LAUNCH_ERROR);
    assert(h9_main_task_launch(&hooks, 0x01E66748u, 0u,
                               copied_core_name) == 0);
    assert(capture.calls == 1u && capture.failures == 0u &&
           capture.entry == 0x01E66748u && capture.argument == 0u &&
           capture.byte_4 == 1u && capture.halfword_6 == 0x0380u &&
           capture.halfword_8 == 0x0300u &&
           capture.name == copied_core_name);
    assert(h9_main_task_launch(&hooks, 0x01E88280u, 0u, "my_app") == 0);
    assert(capture.calls == 2u && capture.entry == 0x01E88280u &&
           capture.byte_4 == 2u && capture.halfword_6 == 0x0800u &&
           capture.halfword_8 == 0x0300u);
    capture.result = 1;
    assert(h9_main_task_launch(&hooks, 1u, 0u, "app_core") ==
           H9_MAIN_TASK_LAUNCH_ERROR);
    assert(capture.calls == 3u && capture.failures == 1u);
    assert(h9_main_task_launch(&hooks, 1u, 0u, "missing") ==
           H9_MAIN_TASK_LAUNCH_ERROR);
    assert(capture.calls == 3u && capture.failures == 2u);
    h9_main_task_hooks_bind_target(&target);
    assert(target.context == NULL && target.create != NULL &&
           target.report_failure == NULL);
}

static void test_main_core_list(void)
{
    uint32_t words[3] = { 0u, 0u, 0xA5A5A5A5u };
    H9MainCoreList list = { words, 0x119ECu };
    H9MainCoreList target;
    h9_main_core_list_bind_target(&target);
    assert((uintptr_t)target.words == 0x119ECu &&
           target.self_address == 0x119ECu);
    assert(h9_main_core_list_init(NULL) != 0);
    list.words = NULL;
    assert(h9_main_core_list_init(&list) != 0);
    list.words = words;
    assert(h9_main_core_list_init(&list) == 0);
    assert(words[0] == 0x119ECu && words[1] == 0x119ECu &&
           words[2] == 0xA5A5A5A5u);
}

int main(void)
{
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    assert(h9_aprs_clock_cfgr_value(0xABCDFFFFu, 0xE123u) == 0xABCD2123u);
    test_wire_vectors();
    test_aprs_systick();
    test_aprs_boot_banner();
    test_aprs_uart2_maintenance();
    test_wire_stream_maximum();
    test_main_aprs_wire_envelope();
    test_main_aprs_wire_receiver();
    test_main_ax25_packet();
    test_main_received_aprs_packet();
    test_main_gps_sentence_frames();
    test_main_gps_fix_parser();
    test_main_rf_output_registers();
    test_main_speaker_gate();
    test_main_audio_gate_arbiter();
    test_main_rf_register_values();
    test_main_gpio_query_reply();
    test_main_control_frames();
    test_main_control_volume();
    test_main_control_07_timer();
    test_main_uart_ring();
    test_main_aprs_session();
    test_aprs_burst_activity_link();
    test_main_policy();
    test_main_rf_live_route();
    test_main_tx_frequency_precheck();
    test_main_tx_service();
    test_main_boot_record();
    test_main_boot_validator();
    test_main_heap_bitmap();
    test_main_memory_window();
    test_main_board_markers();
    test_main_clock_probe();
    test_main_clock_selector();
    test_main_clock_band();
    test_main_clock_calculation();
    test_main_clock_match();
    test_main_clock_timing();
    test_main_clock_apply_first();
    test_main_clock_rom_mode();
    test_main_clock_source_registers();
    test_main_clock_aux_fields();
    test_main_clock_finish();
    test_main_post_clock_rom();
    test_main_post_clock_classification();
    test_main_post_clock_bitmap();
    test_main_post_clock_handoff();
    test_main_post_clock_descriptor_route();
    test_main_post_clock_gate();
    test_main_post_clock_timer_stage();
    test_main_irq_registration();
    test_main_clock_handshake();
    test_main_post_map_gate();
    test_main_post_map_window();
    test_main_task_registry();
    test_main_core_list();
    test_main_timer3_irq();
    test_main_exception_capture();
    test_main_serial_profile();
    test_main_serial_bus();
    test_main_serial_commands();
    test_main_serial_port();
    test_main_serial_dma_read();
    test_main_uart1_irq();
    test_main_uart1_tx();
    test_main_uart1_tx_queue();
    test_main_aprs_uart1_link();
    test_main_aprs_uart1_link_forwards_gps_fix();
    test_main_uart1_board_open();
    test_main_uart1_baud();
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
    test_aprs_tx_interrupt_handoff();
    test_aprs_tx_runtime();
    test_aprs_controller();
    test_aprs_uart_receive_batches();
    test_aprs_uart1_dma_interrupts();
    test_aprs_uart1_board_setup();
    test_aprs_uart2_board_setup_and_raw_write();
    test_aprs_uart2_recover_after_raw_timeout();
    test_aprs_runtime_status_recovery_counters();
    test_aprs_uart2_irq_forward();
    test_aprs_uart2_dma_forward();
    test_aprs_gpio_board();
    test_aprs_wave_output();
    test_aprs_adc_board();
    test_aprs_sample_timer();
    test_aprs_adc_sample_irq();
    test_aprs_adc_full_irq();
    test_aprs_internal_clock_selection();
    test_aprs_uart1_reply_transmit();
    return 0;
}
