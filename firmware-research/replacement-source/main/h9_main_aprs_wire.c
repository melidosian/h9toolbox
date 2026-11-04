#include "h9_main_aprs_wire.h"
#include "h9_aprs_wire.h"

size_t h9_main_aprs_wire_encode(uint8_t command, const uint8_t *payload,
                                 size_t length, uint8_t *output,
                                 size_t capacity)
{
    size_t index;
    size_t used = 0;
    if (payload == NULL || output == NULL ||
        length > H9_MAIN_APRS_MAX_PAYLOAD ||
        capacity < 2u * length + 10u) return 0;
    for (index = 0; index < 5u; ++index)
        output[used++] = H9_APRS_FRAME_END;
    output[used++] = command;
    for (index = 0; index < length; ++index) {
        uint8_t byte = payload[index];
        if (byte == H9_APRS_FRAME_END || byte == H9_APRS_FRAME_ESCAPE) {
            output[used++] = H9_APRS_FRAME_ESCAPE;
            output[used++] = byte == H9_APRS_FRAME_END ? 0xDCu : 0xDDu;
        } else {
            output[used++] = byte;
        }
    }
    for (index = 0; index < 3u; ++index)
        output[used++] = H9_APRS_FRAME_END;
    return used;
}
