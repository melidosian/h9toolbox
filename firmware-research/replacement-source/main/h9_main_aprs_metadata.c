#include "h9_main_aprs_metadata.h"

#include "h9_main_aprs_wire.h"

#include <string.h>

void h9_main_aprs_versions_init(H9MainAprsVersions *versions)
{
    if (versions == NULL) return;
    memset(versions, 0, sizeof *versions);
}

static int copy_version(char output[H9_MAIN_APRS_VERSION_CAPACITY],
                        const uint8_t *input, size_t length,
                        size_t *consumed)
{
    size_t index;
    for (index = 0; index < length && index < H9_MAIN_APRS_VERSION_CAPACITY;
         ++index) {
        if (input[index] == 0) {
            memcpy(output, input, index);
            output[index] = 0;
            *consumed = index + 1u;
            return 1;
        }
    }
    return 0;
}

int h9_main_aprs_version_from_frame(const H9MainWireReceiver *receiver,
                                    H9MainAprsVersions *versions)
{
    const uint8_t *data;
    size_t length;
    size_t consumed;
    char module[H9_MAIN_APRS_VERSION_CAPACITY] = { 0 };
    char protocol[H9_MAIN_APRS_VERSION_CAPACITY] = { 0 };
    if (receiver == NULL || versions == NULL || !receiver->frame_ready ||
        receiver->command != 0x21u || receiver->length < 2u) return 0;
    data = receiver->payload + 1u;
    length = receiver->length - 1u;
    switch (receiver->payload[0]) {
    case 0:
        if (!copy_version(module, data, length, &consumed) ||
            consumed == length ||
            !copy_version(protocol, data + consumed, length - consumed,
                          &consumed)) return 0;
        memcpy(versions->module, module, sizeof module);
        memcpy(versions->protocol, protocol, sizeof protocol);
        versions->has_module = 1;
        versions->has_protocol = 1;
        return 1;
    case 1:
        if (!copy_version(module, data, length, &consumed)) return 0;
        memcpy(versions->module, module, sizeof module);
        versions->has_module = 1;
        return 1;
    case 2:
        if (!copy_version(protocol, data, length, &consumed)) return 0;
        memcpy(versions->protocol, protocol, sizeof protocol);
        versions->has_protocol = 1;
        return 1;
    default:
        return 0;
    }
}

void h9_main_gps_init(H9MainGpsCollector *collector)
{
    if (collector != NULL) collector->length = 0;
}

size_t h9_main_gps_feed(H9MainGpsCollector *collector, const uint8_t *data,
                        size_t length, H9MainGpsSentence on_sentence,
                        void *context)
{
    size_t index;
    size_t emitted = 0;
    if (collector == NULL || (data == NULL && length != 0)) return 0;
    for (index = 0; index < length; ++index) {
        uint8_t byte = data[index];
        if (byte == '$') {
            collector->sentence[0] = byte;
            collector->length = 1;
            continue;
        }
        if (collector->length == 0) continue;
        if (collector->length >= H9_MAIN_GPS_BUFFER_CAPACITY) {
            collector->length = 0;
            continue;
        }
        collector->sentence[collector->length++] = byte;
        if (byte == '\n') {
            if (collector->length > 5u && collector->length <= 255u) {
                collector->sentence[collector->length] = 0;
                if (on_sentence != NULL)
                    on_sentence(context, collector->sentence,
                                collector->length);
                emitted++;
                collector->length = 0;
            } else if (collector->length > 255u) {
                collector->length = 0;
            }
        } else if (collector->length > 1022u) {
            collector->length = 0;
        }
    }
    return emitted;
}

size_t h9_main_aprs_gps_from_frame(H9MainGpsCollector *collector,
                                   const H9MainWireReceiver *receiver,
                                   H9MainGpsSentence on_sentence,
                                   void *context)
{
    if (receiver == NULL || !receiver->frame_ready ||
        receiver->command != 0x10u) return 0;
    return h9_main_gps_feed(collector, receiver->payload, receiver->length,
                            on_sentence, context);
}

size_t h9_main_aprs_gpio_reply_from_frame(const H9MainWireReceiver *receiver,
                                           uint8_t *output, size_t capacity)
{
    static const uint8_t reply[] = { 3u, 0u };
    if (receiver == NULL || !receiver->frame_ready ||
        receiver->command != 0x11u || receiver->length == 0u ||
        receiver->payload[0] != 3u) return 0;
    return h9_main_aprs_wire_encode(0x11u, reply, sizeof reply, output,
                                     capacity);
}
