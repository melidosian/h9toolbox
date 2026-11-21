#include "app_theme.h"
#include "radio_bluetooth.h"

int radio_bt_read(unsigned offset, void *dst, unsigned length)
{
    if ((offset != RADIO_BT_MIC && offset != RADIO_BT_SPEAKER) || length != 1 || !dst) return RADIO_INVALID;
    *(uint8_t *)dst = radio_bt_platform.internal(offset == RADIO_BT_SPEAKER);
    return RADIO_OK;
}

int radio_bt_write(unsigned offset, const void *src, unsigned length)
{
    if ((offset != RADIO_BT_MIC && offset != RADIO_BT_SPEAKER) || length != 1 || !src) return RADIO_INVALID;
    unsigned value = *(const uint8_t *)src, speaker = offset == RADIO_BT_SPEAKER;
    if (value > 1) return RADIO_INVALID;
    int result = radio_bt_platform.set_internal(speaker, value);
    if (result) return result;
    return radio_bt_platform.internal(speaker) == value ? RADIO_OK : RADIO_VERIFY_ERROR;
}

int radio_bt_info(void)
{
    char name[33];
    int result = radio_bt_platform.identity(name);
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
    ui_title("BLUETOOTH IDENTITY");
    gfx_text(3, 25, "NAME", 4, 1, UI_DIM, UI_BG);
    if (result) gfx_text(3, 39, "NAME READ FAILED", 26, 1, UI_DANGER, UI_BG);
    else {
        gfx_text(3, 39, name, 26, 1, UI_TEXT, UI_BG);
        unsigned length = 0; while (name[length] && length < 32) ++length;
        if (length > 26) gfx_text(3, 49, name + 26, 6, 1, UI_TEXT, UI_BG);
    }
    gfx_text(3, 72, "PAIRING PIN", 11, 1, UI_DIM, UI_BG);
    /* The native BT PIN Code row displays this fixed ROM value, not a saved
     * editable setting (app+0x6CB68 -> ROM string app+0xB1741). */
    gfx_text(3, 86, "000000", 6, 1, UI_TEXT, UI_BG);
    ui_footer("MENU / BACK RETURN");
    gfx_wait_release();
    for (;;) { unsigned key = gfx_wait_key(); if (key == H9_KEY_MENU || key == H9_KEY_BACK) return result; }
}

#ifndef RADIO_BT_TICK
#define RADIO_BT_TICK (*(const volatile uint32_t *)0x1D8B0u)
#endif

static void pairing_draw(unsigned saved, unsigned count, unsigned selected, unsigned phase, const char *notice)
{
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
    ui_title(saved ? "PAIRED DEVICES" : "BLUETOOTH DISCOVERY");
    unsigned top = selected > 2 ? selected - 2 : 0;
    for (unsigned row = 0; row < 6 && top + row < count; ++row) {
        struct radio_bt_device device;
        unsigned index = top + row, chosen = index == selected;
        uint16_t bg = chosen ? UI_ACCENT : UI_BG;
        gfx_fill(0, 23 + row * 13, H9_WIDTH, 13, bg);
        if (radio_bt_platform.device(index, &device)) {
            gfx_text(3, 26 + row * 13, "DEVICE READ FAILED", 26, 1, chosen ? UI_BG : UI_DANGER, bg);
        } else {
            if (!device.name[0]) {
                static const char hex[] = "0123456789ABCDEF";
                for (unsigned i = 0; i < 6; ++i) {
                    device.name[i * 3] = hex[device.address[i] >> 4];
                    device.name[i * 3 + 1] = hex[device.address[i] & 15];
                    device.name[i * 3 + 2] = i < 5 ? ':' : 0;
                }
            }
            gfx_text(3, 26 + row * 13, device.name, 26, 1, chosen ? UI_BG : UI_TEXT, bg);
        }
    }
    if (!count) gfx_text(3, 42, saved ? "NO PAIRED DEVICES" : "SEARCHING...", 26, 1, UI_DIM, UI_BG);
    if (notice) gfx_text(3, 105, notice, 26, 1, UI_HINT, UI_BG);
    ui_footer(phase == 2 ? "MENU / BACK RETURN" : !count ? "MENU RETRY  BACK RETURN" : "MENU CONNECT  BACK RETURN");
}

int radio_bt_pairing(void)
{
    unsigned source = 0;
    for (;;) {
        gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
        ui_title("BLUETOOTH PAIRING");
        gfx_text(3, 30, "SEARCH DEVICES", 26, 1, source == 0 ? UI_ACCENT : UI_TEXT, UI_BG);
        gfx_text(3, 47, "PAIRED DEVICES", 26, 1, source == 1 ? UI_ACCENT : UI_TEXT, UI_BG);
        ui_footer("UP/DN  MENU PICK  BACK");
        unsigned key = gfx_wait_key();
        if (key == H9_KEY_BACK) return RADIO_OK;
        if (key == H9_KEY_UP || key == H9_KEY_DOWN) { source ^= 1; continue; }
        if (key != H9_KEY_MENU) continue;
        int result = radio_bt_platform.open(source);
        if (result) return result;
        unsigned selected = 0, connecting = 0, sample = 0, stable = 0;
        const char *notice = 0;
        uint32_t last_draw = RADIO_BT_TICK - 100u, began = RADIO_BT_TICK;
        gfx_wait_release();
        for (;;) {
            unsigned count = radio_bt_platform.count();
            if (count > 10) count = 10;
            if (selected >= count) selected = 0;
            if (connecting && radio_bt_platform.connected()) {
                notice = "CONNECTED";
                connecting = 2;
            } else if (connecting == 2) {
                notice = "DISCONNECTED"; connecting = 0;
            } else if (connecting == 1 && RADIO_BT_TICK - began >= 3000) {
                notice = "CONNECTION TIMED OUT"; connecting = 0;
            }
            if (RADIO_BT_TICK - last_draw >= 20u) {
                pairing_draw(source, count, selected, connecting, notice);
                last_draw = RADIO_BT_TICK;
            }
            key = h9_key();
            if (key == sample && key != stable) {
                stable = key;
                if (key == H9_KEY_BACK || (key == H9_KEY_MENU && connecting == 2)) {
                    radio_bt_platform.close();
                    gfx_wait_release();
                    return RADIO_OK;
                }
                if (connecting != 1 && (key == H9_KEY_UP || key == H9_KEY_DOWN) && count) {
                    selected = key == H9_KEY_UP ? (selected ? selected - 1 : count - 1) : (selected + 1) % count;
                    notice = 0; last_draw -= 20u;
                } else if (key == H9_KEY_MENU && !connecting && count) {
                    notice = "CONNECTING...";
                    pairing_draw(source, count, selected, 1, notice);
                    result = radio_bt_platform.connect(selected);
                    if (result) notice = "CONNECTION FAILED";
                    else { connecting = 1; began = RADIO_BT_TICK; }
                    last_draw -= 20u;
                } else if (key == H9_KEY_MENU && !connecting && !count) {
                    radio_bt_platform.close();
                    result = radio_bt_platform.open(source);
                    if (result) return result;
                    notice = 0; began = RADIO_BT_TICK; last_draw -= 20u;
                }
            }
            sample = key;
            h9_delay(20);
        }
    }
}

int radio_bt_reset(void) { return radio_bt_platform.reset(); }

#ifndef RADIO_BT_HOST
#define BT_CFG8(offset) (*(volatile uint8_t *)(uintptr_t)(0xC0F0u + (offset)))
#define BT_CORE32(offset) (*(volatile uint32_t *)(uintptr_t)(0x11AD0u + (offset)))
#define BT_NOR_READY (*(const volatile uint16_t *)0x12234u == 0x5AA5)
int radio_bt_vm_read(unsigned key, void *dst, unsigned length);
void radio_bt_mic_native(unsigned on);
void radio_bt_speaker_native(unsigned on);
void *radio_bt_alloc_native(unsigned length);
void radio_bt_role_native(void);
void radio_bt_discover_native(void);
unsigned radio_bt_saved_native(void);
void radio_bt_connect_native(unsigned index);
void radio_bt_cleanup_native(void);
void radio_bt_command_native(unsigned command, unsigned length, const void *data);
void radio_bt_reset_native(void);
int radio_bt_partition_native(unsigned partition, unsigned offset, void *dst, unsigned length);

static int native_identity(char name[33])
{
    for (unsigned i = 0; i < 33; ++i) name[i] = 0;
    int read = radio_bt_vm_read(101, name, 32);
    if (read <= 0) return RADIO_IO_ERROR;
    name[32] = 0;
    for (unsigned i = 0; i < 32 && name[i]; ++i)
        if ((uint8_t)name[i] < 32 || (uint8_t)name[i] > 126) name[i] = '?';
    return RADIO_OK;
}
static unsigned native_internal(unsigned speaker) { return BT_CFG8(speaker ? 204 : 203); }
static int native_set_internal(unsigned speaker, unsigned on)
{
    if (speaker) radio_bt_speaker_native(on);
    else radio_bt_mic_native(on);
    return RADIO_OK;
}
struct native_bt_entry { uint32_t name; uint8_t length, pad[3]; uint32_t address; };
static int native_open(unsigned saved)
{
    if (!BT_CFG8(192) || !BT_NOR_READY) return RADIO_BUSY;
    if (BT_CORE32(532) || BT_CFG8(199)) return RADIO_BUSY;
    uint8_t *pool = radio_bt_alloc_native(980);
    if (!pool) return RADIO_IO_ERROR;
    for (unsigned i = 0; i < 980; ++i) pool[i] = 0;
    struct native_bt_entry *entries = (struct native_bt_entry *)pool;
    for (unsigned i = 0; i < 10; ++i) {
        entries[i].name = (uint32_t)(uintptr_t)(pool + 120 + i * 40);
        entries[i].address = (uint32_t)(uintptr_t)(pool + 520 + i * 6);
        BT_CORE32(2324 + i * 4) = (uint32_t)(uintptr_t)(pool + 580 + i * 40);
    }
    BT_CORE32(532) = BT_CORE32(536) = (uint32_t)(uintptr_t)pool;
    BT_CFG8(199) = saved ? 2 : 1;
    BT_CFG8(198) = 0;
    if (saved) radio_bt_saved_native();
    else { radio_bt_role_native(); radio_bt_discover_native(); }
    return RADIO_OK;
}
static unsigned native_count(void) { unsigned n = BT_CFG8(198); return n <= 10 ? n : 0; }
static int native_device(unsigned index, struct radio_bt_device *device)
{
    if (index >= native_count() || !BT_CORE32(536)) return RADIO_INVALID;
    const struct native_bt_entry *entries = (const struct native_bt_entry *)(uintptr_t)BT_CORE32(536);
    const struct native_bt_entry *entry = entries + index;
    if (!entry->name || !entry->address) return RADIO_IO_ERROR;
    unsigned length = entry->length <= 39 ? entry->length : 39;
    const uint8_t *name = (const uint8_t *)(uintptr_t)entry->name;
    for (unsigned i = 0; i < length; ++i) device->name[i] = name[i] >= 32 && name[i] <= 126 ? name[i] : '?';
    device->name[length] = 0;
    const uint8_t *address = (const uint8_t *)(uintptr_t)entry->address;
    for (unsigned i = 0; i < 6; ++i) device->address[i] = address[i];
    return RADIO_OK;
}
static int native_connect(unsigned index)
{
    if (index >= native_count() || !BT_CORE32(536)) return RADIO_INVALID;
    BT_CFG8(194) = 1;
    /* This is the native connect-and-remember backend, not its old UI. It
     * stores up to ten paired peers through the normal partition codec. */
    radio_bt_connect_native(index);
    return RADIO_OK;
}
static unsigned native_connected(void) { return BT_CFG8(194) == 2; }
static void native_close(void)
{
    /* Disable asynchronous discovery writes before the pool is released. */
    BT_CFG8(199) = 0;
    radio_bt_command_native(17, 0, 0);
    radio_bt_cleanup_native();
}
static int native_reset(void)
{
    if (!BT_NOR_READY || BT_CORE32(532)) return RADIO_BUSY;
    radio_bt_reset_native();
    static const uint8_t expected[] = {0, 0, 3, 2, 0, 0};
    uint8_t stored[6];
    if (radio_bt_partition_native(4, 7969, stored, 6)) return RADIO_IO_ERROR;
    for (unsigned i = 0; i < 6; ++i) if (stored[i] != expected[i]) return RADIO_VERIFY_ERROR;
    if (BT_CFG8(192) || BT_CFG8(193) || BT_CFG8(205) != 3 || BT_CFG8(206) != 2 || BT_CFG8(207) || BT_CFG8(208))
        return RADIO_VERIFY_ERROR;
    return RADIO_OK;
}
const struct radio_bt_platform radio_bt_platform = {
    native_identity, native_internal, native_set_internal, native_open, native_count,
    native_device, native_connect, native_connected, native_close, native_reset
};
#endif
