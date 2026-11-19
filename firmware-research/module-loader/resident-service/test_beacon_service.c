#include "beacon_service.h"
#include "dispatch.h"
#include "modem_input.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct h9_bs service;
static struct h9_rs_state dispatch;
static struct h9_rs_binding binding;
static void poll(void *context, uint32_t now_ms)
{
    assert(context == &service);
    h9_bs_poll(&service, now_ms);
}
static void tick(uint32_t now_ms)
{
    assert(h9_rs_poll(&dispatch, &binding, H9_RS_REQUIRED, now_ms) == H9_RS_OK);
}
static const uint8_t position[] =
    "\xc0\x10$GNRMC,141500.00,A,4152.6800,N,08737.8000,W,12.4,275.5,111126,,,A\r\n\xc0";

static void rmc(uint32_t now_ms)
{
    int seen = 0;
    for (unsigned i = 0; i < sizeof position - 1; ++i)
        seen |= h9_bs_feed_byte(&service, position[i], now_ms);
    assert(seen);
}

static void write_dma(uint8_t *buffer, uint32_t *head, const uint8_t *bytes, unsigned len)
{
    for (unsigned i = 0; i < len; ++i)
        buffer[((*head)++) & (H9_MODEM_DMA_CAPACITY - 1u)] = bytes[i];
}

static void test_modem_input(void)
{
    uint8_t buffer[H9_MODEM_DMA_CAPACITY] = {0};
    uint32_t head = UINT32_MAX - 9;
    struct h9_modem_cursor cursor = {0};
    h9_bs_init(&service);
    assert(h9_bs_arm(&service, 1, 1, 0, 0));
    h9_modem_poll(&cursor, &service, buffer, &head, 0);
    write_dma(buffer, &head, position, 20);  /* incomplete frame across DMA and counter wrap */
    h9_modem_poll(&cursor, &service, buffer, &head, 200);
    assert(!service.status.gps.seen && cursor.next == head);
    write_dma(buffer, &head, position + 20, sizeof position - 1 - 20);
    h9_modem_poll(&cursor, &service, buffer, &head, 300);
    assert(service.status.gps.fix && !strcmp(service.status.gps.lat, "4152.68N"));

    write_dma(buffer, &head, position, 20);
    h9_modem_poll(&cursor, &service, buffer, &head, 320);
    head += H9_MODEM_DMA_CAPACITY;
    h9_modem_poll(&cursor, &service, buffer, &head, 340);
    assert(!service.status.gps.fix && !service.feed.started && cursor.next == head);
    write_dma(buffer, &head, position + 20, sizeof position - 1 - 20);
    h9_modem_poll(&cursor, &service, buffer, &head, 360);
    assert(!service.status.gps.fix);
    write_dma(buffer, &head, position, sizeof position - 1);
    h9_modem_poll(&cursor, &service, buffer, &head, 15000);
    h9_bs_poll(&service, 15000);
    assert(service.status.gps.fix && service.status.pending);
    h9_modem_poll(&cursor, &service, NULL, &head, 15020);
    assert(!cursor.attached && !service.status.gps.fix && service.status.pending);
}

int main(void)
{
    struct h9_bs_status live;
    h9_bs_init(&service);
    h9_rs_init(&dispatch);
    binding = (struct h9_rs_binding){ H9_RS_MAGIC, H9_RS_ABI,
        sizeof binding, H9_RS_REQUIRED, poll, &service };
    assert(h9_bs_status(&service, &live) && live.size == sizeof live && !live.armed);
    assert(!h9_bs_arm(&service, 1, 0, 0, 0));
    rmc(0);
    tick(10000);
    assert(h9_bs_status(&service, &live) && !live.armed && !live.gps.fix && live.gps.lat[0]);
    rmc(0);
    assert(h9_bs_arm(&service, 0, 5, 0, 0));
    tick(14999);
    assert(h9_bs_status(&service, &live) && !live.pending && !live.gps.fix);
    rmc(15000);
    tick(15000);
    assert(h9_bs_status(&service, &live) && live.armed && live.pending &&
           live.wait == H9_BS_READY && live.gps.fix && live.pending_id == 1);
    uint32_t generation = live.generation, pending_id = live.pending_id;
    assert(!h9_bs_wait_for(&service, generation - 1, pending_id, H9_BS_BUSY));
    assert(h9_bs_wait_for(&service, generation, pending_id, H9_BS_BUSY));
    assert(h9_bs_status(&service, &live) && live.wait == H9_BS_BUSY);
    assert(!h9_bs_submitted(&service, generation, pending_id + 1, 1, 20000));
    assert(h9_bs_submitted(&service, generation, pending_id, 0, 20000));
    assert(!h9_bs_submitted(&service, generation, pending_id, 1, 20001));
    tick(79999);
    assert(h9_bs_status(&service, &live) && live.pending &&
           live.wait == H9_BS_RETRY && !live.submitted);
    tick(80000);
    assert(h9_bs_status(&service, &live) && live.pending &&
           live.wait == H9_BS_READY);
    assert(h9_bs_submitted(&service, generation, pending_id, 1, 80000));
    assert(h9_bs_status(&service, &live) && !live.pending &&
           live.submitted == 1 && live.last_submit_ms == 80000);

    /* Replace arbitrary menu/screen scratch: shared service state survives. */
    uint8_t module_scratch[384];
    memset(module_scratch, 0xA5, sizeof module_scratch);
    rmc(139999);
    tick(139999);
    assert(h9_bs_status(&service, &live) && live.armed && !live.pending);
    tick(140000);
    assert(h9_bs_status(&service, &live) && live.pending && live.submitted == 1);
    h9_bs_stop(&service);
    assert(h9_bs_status(&service, &live) && !live.armed && !live.pending);
    assert(!h9_bs_submitted(&service, generation, live.pending_id, 1, 140000));

    /* Re-arm near 32-bit clock rollover; a stale token from the prior arm
     * cannot acknowledge the new event, and retry uses unsigned elapsed. */
    uint32_t start = UINT32_MAX - 15000u;
    rmc(start);
    assert(h9_bs_arm(&service, 1, 1, 1, start));
    tick(start + 15000u);
    assert(h9_bs_status(&service, &live) && live.pending &&
           live.generation != generation);
    assert(!h9_bs_submitted(&service, generation, live.pending_id, 1, 0));
    generation = live.generation;
    pending_id = live.pending_id;
    assert(h9_bs_submitted(&service, generation, pending_id, 0, UINT32_MAX));
    tick(59998);
    assert(h9_bs_status(&service, &live) && live.wait == H9_BS_RETRY);
    tick(59999);
    assert(h9_bs_status(&service, &live) && live.wait == H9_BS_READY);
    service.status.magic ^= 1;
    assert(!h9_bs_status(&service, &live));
    test_modem_input();
    puts("beacon service: shared status, pending, retry, rollover and UART gaps PASS (no RF)");
    return 0;
}
