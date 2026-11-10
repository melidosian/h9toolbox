/* Stock Beacon List loader (linked into the station list only): sys_data
 * `aprs_beacon` at chip 0x80B600, u16 count, u16 next index, 100 x 328-byte
 * records {u16 328, u8 0, u8 0, 324-byte decoded packet}. Read only. */
#include "aprs_core.h"

#define BEACONS 0x80B600u
enum { BEACON_SLOTS = 100, BEACON_RECORD = 328, RECORD = 324 };
enum { R_DEST = 0, R_SRC = 7, R_INFO = 59, R_LEN = 316 };

void aprs_load_stored(const struct h9_api *api)
{
    uint8_t head[4];
    api->read(head, BEACONS, 4);
    unsigned count = head[0] | (unsigned)head[1] << 8, next = head[2] | (unsigned)head[3] << 8;
    if (count > BEACON_SLOTS || next >= BEACON_SLOTS) return;     /* erased or unknown */
    uint8_t *r = A->frame;                                        /* free on a fresh start */
    for (unsigned k = 0; k < count; k++) {
        unsigned slot = (next + BEACON_SLOTS - 1 - k) % BEACON_SLOTS;
        uint32_t at = BEACONS + 6 + slot * BEACON_RECORD;
        api->read(head, at, 4);
        if ((head[0] | (unsigned)head[1] << 8) != BEACON_RECORD) continue;
        /* A->frame is FRAME_MAX (200) bytes, smaller than a record: read the
         * addresses and as much of the info field as fits. (Reading all 324
         * bytes overran into the key and redraw state, 2026-11-10.) */
        api->read(head, at + 4 + R_LEN, 2);
        unsigned len = head[0] | (unsigned)head[1] << 8;
        if (len > FRAME_MAX - R_INFO) len = FRAME_MAX - R_INFO;
        api->read(r, at + 4, R_INFO + len);
        char dest[7], from[CALL];
        for (unsigned i = 0; i < 6; i++) dest[i] = (char)(r[R_DEST + i] ? r[R_DEST + i] : ' ');
        dest[6] = 0;
        aprs_call_of(r + R_SRC, 0, from);
        aprs_packet(dest, from, r + R_INFO, len, 0);
    }
    A->length = 0;
}

