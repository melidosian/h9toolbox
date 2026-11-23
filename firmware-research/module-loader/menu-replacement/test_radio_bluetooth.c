#define H9_GFX_TEST_STUB
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "h9mod.h"
#include "radio_bluetooth.h"

static uint32_t tick;
#define RADIO_BT_TICK tick
#define RADIO_BT_HOST
#include "radio_bluetooth.c"

static uint8_t internal[2];
static unsigned opened, closed, connected, chosen, resets, last_source, deny_open, fail_internal, no_connect;
static unsigned saw_pin, saw_peer, saw_connected, saw_empty, saw_timeout;
static const unsigned *blocking_keys;
struct press { unsigned at, key; };
static const struct press *presses;

static int identity(char name[33]) { memcpy(name, "TD-H9 TEST RADIO", 17); return RADIO_OK; }
static unsigned get_internal(unsigned speaker) { assert(speaker < 2); return internal[speaker]; }
static int set_internal(unsigned speaker, unsigned value)
{ assert(speaker < 2); if (!fail_internal) internal[speaker] = value; return RADIO_OK; }
static int open_devices(unsigned saved) { last_source = saved; if (deny_open) return RADIO_BUSY; ++opened; return RADIO_OK; }
static unsigned device_count(void) { return last_source ? 0 : tick >= 20 ? 2 : 0; }
static int device(unsigned index, struct radio_bt_device *peer)
{
    assert(index < 2);
    memset(peer, 0, sizeof *peer);
    strcpy(peer->name, index ? "HEADSET TWO" : "HEADSET ONE");
    return RADIO_OK;
}
static int connect_device(unsigned index) { assert(index < 2); chosen = index; connected = 1; return RADIO_OK; }
static unsigned connection_status(void) { return connected && tick >= 200 && !no_connect; }
static void close_devices(void) { ++closed; }
static int reset_devices(void) { ++resets; return RADIO_OK; }
const struct radio_bt_platform radio_bt_platform = {
    identity, get_internal, set_internal, open_devices, device_count, device,
    connect_device, connection_status, close_devices, reset_devices
};

unsigned gfx_wait_key(void) { unsigned key = *blocking_keys++; assert(key); return key; }
void gfx_wait_release(void) { }
unsigned h9_key(void)
{
    for (const struct press *p = presses; p && p->at; ++p)
        if (tick >= p->at && tick < p->at + 40) return p->key;
    return 0;
}
void h9_delay(unsigned ms) { tick += ms / 10; assert(tick < 5000); }
void gfx_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t color)
{ (void)color; assert(x + w <= H9_WIDTH && y + h <= H9_HEIGHT); }
unsigned gfx_text(unsigned x, unsigned y, const char *s, unsigned max, unsigned scale, uint16_t fg, uint16_t bg)
{
    (void)fg; (void)bg;
    unsigned n = 0; while (n < max && s[n]) ++n;
    assert(x + n * 6 * scale <= H9_WIDTH && y + 8 * scale <= H9_HEIGHT);
    saw_pin |= !strcmp(s, "000000");
    saw_peer |= !strcmp(s, "HEADSET TWO");
    saw_connected |= !strcmp(s, "CONNECTED");
    saw_empty |= !strcmp(s, "NO PAIRED DEVICES");
    saw_timeout |= !strcmp(s, "CONNECTION TIMED OUT");
    return x + n * 6 * scale;
}

int main(void)
{
    uint8_t value = 1, actual = 0;
    assert(radio_bt_write(RADIO_BT_MIC, &value, 1) == RADIO_OK);
    assert(radio_bt_read(RADIO_BT_MIC, &actual, 1) == RADIO_OK && actual == 1 && !internal[1]);
    assert(radio_bt_write(RADIO_BT_SPEAKER, &value, 1) == RADIO_OK);
    assert(radio_bt_read(RADIO_BT_SPEAKER, &actual, 1) == RADIO_OK && actual == 1);
    value = 2;
    assert(radio_bt_write(RADIO_BT_MIC, &value, 1) == RADIO_INVALID && internal[0] == 1);
    assert(radio_bt_read(RADIO_BT_SPEAKER + 1, &actual, 1) == RADIO_INVALID);
    assert(radio_bt_write(RADIO_BT_MIC, &value, 2) == RADIO_INVALID);
    fail_internal = 1; value = 0;
    assert(radio_bt_write(RADIO_BT_MIC, &value, 1) == RADIO_VERIFY_ERROR);
    fail_internal = 0;
    const unsigned info[] = { H9_KEY_BACK };
    blocking_keys = info;
    assert(radio_bt_info() == RADIO_OK && saw_pin);

    const unsigned discover[] = { H9_KEY_MENU };
    const struct press pair[] = {{40,H9_KEY_DOWN},{120,H9_KEY_MENU},{300,H9_KEY_BACK},{0,0}};
    blocking_keys = discover; presses = pair; tick = 0;
    assert(radio_bt_pairing() == RADIO_OK && opened == 1 && closed == 1 && chosen == 1 && saw_peer && saw_connected);

    const unsigned saved[] = { H9_KEY_DOWN, H9_KEY_MENU };
    const struct press leave[] = {{80,H9_KEY_BACK},{0,0}};
    blocking_keys = saved; presses = leave; tick = connected = 0;
    assert(radio_bt_pairing() == RADIO_OK && opened == 2 && closed == 2 && last_source == 1 && saw_empty);

    const struct press timeout[] = {{120,H9_KEY_MENU},{3200,H9_KEY_BACK},{0,0}};
    no_connect = 1; blocking_keys = discover; presses = timeout; tick = connected = 0;
    assert(radio_bt_pairing() == RADIO_OK && opened == 3 && closed == 3 && saw_timeout);
    no_connect = 0;
    const struct press retry[] = {{80,H9_KEY_MENU},{160,H9_KEY_BACK},{0,0}};
    blocking_keys = saved; presses = retry; tick = connected = 0;
    assert(radio_bt_pairing() == RADIO_OK && opened == 5 && closed == 5);

    deny_open = 1; blocking_keys = discover;
    assert(radio_bt_pairing() == RADIO_BUSY && opened == 5 && closed == 5);
    assert(radio_bt_reset() == RADIO_OK && resets == 1);
    puts("Bluetooth inline controls, identity, discovery/paired selection, connection and cleanup passed.");
}
