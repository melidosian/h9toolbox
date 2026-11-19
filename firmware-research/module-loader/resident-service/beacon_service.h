/* Permanent Beacon scheduler logic. No radio, NOR, display, or task calls. */
#ifndef H9_BEACON_SERVICE_H
#define H9_BEACON_SERVICE_H
#include <stdint.h>
#include "../modules/aprs/beacon_core.h"
#include "../modules/aprs/third_party/smartbeacon/smartbeacon.h"

#define H9_BS_MAGIC UINT32_C(0x48534231) /* HSB1 */
#define H9_BS_ABI 1u
enum h9_bs_wait {
    H9_BS_OFF, H9_BS_INTERVAL, H9_BS_READY, H9_BS_RETRY,
    H9_BS_POSITION, H9_BS_BUSY, H9_BS_SIDE, H9_BS_FREQUENCY,
    H9_BS_SETTING, H9_BS_CALLSIGN, H9_BS_TX_ERROR
};
struct h9_bs_status {
    uint32_t magic, generation, pending_id, submitted, last_submit_ms;
    uint16_t abi, size;
    uint8_t armed, timed, minutes, symbol, pending, wait;
    struct beacon_gps gps;
};
struct h9_bs {
    struct h9_bs_status status;
    sb_state_t smart;
    struct beacon_feed feed;
    uint32_t gps_ms, retry_ms;
    uint8_t retry_active;
};
_Static_assert(sizeof(struct h9_bs) <= 384,
               "Beacon service exceeds the first protected APRS RAM half");

/* Initialize only at boot with RF quiescent. Arm/stop preserve the feed cursor
 * and last position, while changing the current scheduler generation. */
void h9_bs_init(struct h9_bs *s);
int h9_bs_arm(struct h9_bs *s, uint8_t timed, uint8_t minutes,
              uint8_t symbol, uint32_t now_ms);
void h9_bs_stop(struct h9_bs *s);
/* Feed copies of bytes from the modem ring with an independent cursor. */
int h9_bs_feed_byte(struct h9_bs *s, uint8_t byte, uint32_t now_ms);
void h9_bs_input_gap(struct h9_bs *s, uint32_t now_ms);
/* May update pending/status only; never calls TX or other board operations. */
void h9_bs_poll(struct h9_bs *s, uint32_t now_ms);
/* A pending identifier must match. accepted means a TX routine returned after
 * submission, not proof of on-air emission or iGate reception. */
int h9_bs_submitted(struct h9_bs *s, uint32_t generation, uint32_t pending_id,
                    int accepted, uint32_t now_ms);
/* Transient refusal keeps the pending event without starting retry timeout. */
int h9_bs_wait_for(struct h9_bs *s, uint32_t generation, uint32_t pending_id,
                   enum h9_bs_wait why);
int h9_bs_status(const struct h9_bs *s, struct h9_bs_status *out);

#endif
