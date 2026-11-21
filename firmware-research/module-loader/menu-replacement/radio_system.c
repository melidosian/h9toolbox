#include "app_theme.h"
#include "radio_system.h"
#include "radio_manual_images.h"
#include "../modules/aprs/aprs_resident_config.h"

struct guarded_call { radio_rf_action action; void *context; int result; };
static void guarded_run(const void *arg, unsigned length)
{
    (void)length;
    struct guarded_call *call = (struct guarded_call *)arg;
    call->result = call->action(call->context);
}
int radio_with_rf(const struct h9_api *loader, radio_rf_action action, void *context)
{
    struct guarded_call call = { action, context, RADIO_BUSY };
#ifdef H9_RESIDENT_TX
    const struct h9_runtime_api *api = aprs_runtime(loader);
    if (!api || !api->stock_tx) return RADIO_BUSY;
    api->stock_tx(&call, 0, guarded_run);
#else
    (void)loader;
    guarded_run(&call, 0);
#endif
    return call.result;
}
static int reload(void *context) { (void)context; return radio_system_platform.reload(); }
int radio_apply_pending(const struct h9_api *loader) { return radio_with_rf(loader, reload, 0); }

int radio_version_feed(struct radio_version_parser *p, unsigned byte)
{
    if (byte == 0xc0) {
        int good = 0;
        if (!p->overflow && !p->escaped && p->count >= 6 && p->bytes[0] == 0x21 && !p->bytes[1]) {
            unsigned at = 2, n = 0;
            char module[16] = {0}, protocol[16] = {0};
            while (at < p->count && p->bytes[at] >= 32 && p->bytes[at] <= 126 && n < 15) module[n++] = p->bytes[at++];
            module[n] = 0;
            if (n && at < p->count && !p->bytes[at++]) {
                n = 0;
                while (at < p->count && p->bytes[at] >= 32 && p->bytes[at] <= 126 && n < 15) protocol[n++] = p->bytes[at++];
                protocol[n] = 0;
                if (n && at < p->count && !p->bytes[at++]) {
                    good = 1;
                    for (unsigned i = 0; i < 16; ++i) { p->module[i] = module[i]; p->protocol[i] = protocol[i]; }
                }
            }
        }
        p->count = p->escaped = p->overflow = 0;
        return good;
    }
    if (p->escaped) {
        p->escaped = 0;
        if (byte == 0xdc) byte = 0xc0;
        else if (byte == 0xdd) byte = 0xdb;
        else { p->overflow = 1; return 0; }
    } else if (byte == 0xdb) { p->escaped = 1; return 0; }
    if (p->count < sizeof p->bytes) p->bytes[p->count++] = byte;
    else p->overflow = 1;
    return 0;
}

static void line(unsigned row, const char *label, const char *value)
{
    unsigned y = 24 + row * 12;
    gfx_text(3, y, label, 10, 1, UI_DIM, UI_BG);
    gfx_text(67, y, value, 15, 1, UI_TEXT, UI_BG);
}
static void wait_back(void)
{
    gfx_wait_release();
    for (;;) { unsigned key = gfx_wait_key(); if (key == H9_KEY_BACK || key == H9_KEY_MENU) return; }
}
static void firmware_version(uint32_t version, char *out)
{
    unsigned used = 0;
    out[used++] = 'V';
    for (unsigned shift = 16;; shift -= 8) {
        char digits[12]; radio_number_label(version >> shift & 255, digits);
        for (unsigned i = 0; digits[i]; ++i) out[used++] = digits[i];
        if (!shift) break;
        out[used++] = '.';
    }
    if (version >> 24) {
        out[used++] = '-'; out[used++] = 'R'; out[used++] = 'C';
        radio_number_label(version >> 24, out + used);
    } else out[used] = 0;
}
static int information(const struct h9_api *loader)
{
    struct radio_system_identity id;
    radio_system_platform.identity(&id);
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
    ui_title("RADIO INFORMATION");
    char version[20];
    firmware_version(loader->firmware, version);
    line(0, "MODEL", "TD-H9");
    line(1, "FIRMWARE", version);
    line(2, "BASE", id.base);
    line(3, "HARDWARE", id.hardware);
    line(4, "BAND", id.region == 1 ? "HAM" : id.region == 2 ? "GMRS" : "NORMAL");
    line(5, "ANI", id.ani);
    ui_footer("MENU / BACK RETURN");
    wait_back();
    return RADIO_OK;
}
static int modem_information(void)
{
    struct radio_version_parser parser = { .count = 0 };
    char station[12];
    uint8_t own[7];
    unsigned n = 0;
    if (!radio_stock_storage.read(0x307d, own, sizeof own)) {
        while (n < 6 && own[n] >= 33 && own[n] <= 126) { station[n] = own[n]; ++n; }
        while (n && station[n-1] == ' ') --n;
        if (n && own[6] < 16) { station[n++] = '-'; radio_number_label(own[6], station+n); }
        else station[n] = 0;
    }
    if (!n) { station[0]='-';station[1]='-';station[2]=0; }
    unsigned cursor = radio_system_platform.ring_end() & 2047;
    unsigned started = radio_system_platform.ticks(), redraw = 1, sample = 0, stable = 0;
    radio_system_platform.query_version();
    gfx_wait_release();
    for (;;) {
        unsigned end = radio_system_platform.ring_end() & 2047;
        while (cursor != end) {
            redraw |= radio_version_feed(&parser, radio_system_platform.ring_byte(cursor));
            cursor = (cursor + 1) & 2047;
        }
        unsigned expired = radio_system_platform.ticks() - started >= 300;
        if (redraw || (!parser.module[0] && expired)) {
            gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
            ui_title("APRS INFORMATION");
            gfx_text(3, 23, "CALLSIGN", 26, 1, UI_DIM, UI_BG);
            gfx_text(3, 34, station, 26, 1, UI_TEXT, UI_BG);
            gfx_text(3, 51, "MODEM VERSION", 26, 1, UI_DIM, UI_BG);
            gfx_text(3, 62, parser.module[0] ? parser.module : expired ? "NO REPLY" : "QUERYING...", 26, 1, UI_TEXT, UI_BG);
            gfx_text(3, 79, "PROTOCOL VERSION", 26, 1, UI_DIM, UI_BG);
            gfx_text(3, 90, parser.protocol[0] ? parser.protocol : "--", 26, 1, UI_TEXT, UI_BG);
            ui_footer("MENU QUERY   BACK RETURN");
            redraw = 0;
            if (expired) started += 300; /* keep the idle display responsive */
        }
        unsigned key = h9_key();
        if (key == sample && key != stable) {
            stable = key;
            if (key == H9_KEY_BACK) { gfx_wait_release(); return RADIO_OK; }
            if (key == H9_KEY_MENU) {
                parser = (struct radio_version_parser){ .count = 0 };
                cursor = radio_system_platform.ring_end() & 2047;
                started = radio_system_platform.ticks();
                radio_system_platform.query_version(); redraw = 1;
            }
        }
        sample = key;
        h9_delay(20);
    }
}
static int manuals(void)
{
    unsigned selected = 0;
    for (;;) {
        const struct radio_manual_image *page = &manual_images[selected];
        gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
        ui_title(page->name);
        h9_lcd_window(45, 23, 114, 92);
        uint8_t pixels[128];
        for (unsigned at = 0; at < 4900;) {
            unsigned count = 4900 - at; if (count > 64) count = 64;
            for (unsigned i = 0; i < count; ++i) {
                unsigned bit = (at + i) * page->bits, shift = bit % 8;
                unsigned index = page->pixels[bit/8] >> shift;
                if (shift + page->bits > 8) index |= (unsigned)page->pixels[bit/8+1] << (8-shift);
                unsigned color = page->palette[index & ((1u << page->bits) - 1)];
                pixels[i*2] = color >> 8; pixels[i*2+1] = color;
            }
            h9_lcd_write(pixels, count*2); at += count;
        }
        gfx_text(3, 103, "SCAN QR WITH YOUR PHONE", 26, 1, UI_HINT, UI_BG);
        ui_footer("UP/DN PAGE    BACK RETURN");
        unsigned key = gfx_wait_key();
        if (key == H9_KEY_BACK) return RADIO_OK;
        if (key == H9_KEY_UP) selected = (selected + 2) % 3;
        else if (key == H9_KEY_DOWN || key == H9_KEY_MENU) selected = (selected + 1) % 3;
    }
}
struct reset_call { const struct h9_api *loader; unsigned action, kind; };
static int do_reset(void *context)
{
    const struct reset_call *call = context;
    if (call->action == RADIO_SYS_RESET_ALL || call->action == RADIO_SYS_RESET_APRS) {
        const struct h9_runtime_api *api = aprs_runtime(call->loader);
        struct h9_runtime_status status;
        if (!api || !api->read_status(&status)) return RADIO_IO_ERROR;
        struct ts_settings defaults = { .minutes = TS_DEFAULT_MINUTES };
        if (!aprs_settings_save(call->loader, api, &defaults, &status.settings)) return RADIO_IO_ERROR;
    }
    return radio_system_platform.reset(call->kind);
}
static int reset(const struct h9_api *loader, unsigned action)
{
    static const char *const titles[] = {"RESET VFOS?", "RESET CHANNELS?", "RESET ANALOG DATA?", "RESET ALL SETTINGS?", "RESET APRS SETTINGS?"};
    unsigned kind = action - RADIO_SYS_RESET_VFO;
    if (kind >= sizeof titles / sizeof *titles) return RADIO_INVALID;
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
    ui_title(titles[kind]);
    gfx_text(3, 36, "RESTORE FACTORY SETTINGS", 26, 1, UI_TEXT, UI_BG);
    gfx_text(3, 55, "THIS CANNOT BE UNDONE", 26, 1, UI_HINT, UI_BG);
    ui_footer("MENU CONFIRM  BACK CANCEL");
    gfx_wait_release();
    for (;;) {
        unsigned key = gfx_wait_key();
        if (key == H9_KEY_BACK) return RADIO_OK;
        if (key == H9_KEY_MENU) break;
    }
    struct reset_call call = { loader, action, kind };
    int result = radio_with_rf(loader, do_reset, &call);
    return result ? result : -1;
}
int radio_system(const struct h9_api *loader, unsigned action)
{
    if (action == RADIO_SYS_INFO) return information(loader);
    if (action == RADIO_SYS_APRS_INFO) return modem_information();
    if (action == RADIO_SYS_MANUAL) return manuals();
    return reset(loader, action);
}
