#include "aprs_resident_config.h"
#include "app_theme.h"

enum { SET_A = 0x3FC000, SET_B = 0x3FD000, GAME_DIRECTORY = 0xA1F000 };
#define NOR_READY (*(const volatile uint16_t *)0x12234u == 0x5AA5u)
#define TX_SETTING (*(const volatile uint8_t *)0xC533u)
#define ACTIVE_SIDE (*(const volatile uint8_t *)0x11B50u >> 7)

static int read_nor(void *context, uint32_t address, void *dst, unsigned size)
{
    if (!NOR_READY) return 0;
    ((const struct h9_api *)context)->read(dst, address, size);
    return 1;
}

static int write_nor(void *context, uint32_t address, const void *src, unsigned size)
{
    (void)context;
    if (!NOR_READY || !(((address == SET_A || address == SET_B) && size == 28) ||
        ((address == SET_A + 28 || address == SET_B + 28) && size == 4))) return 0;
    ((void (*)(const void *, uint32_t, unsigned))0x01E5006Eu)(src, address, size);
    return 1;
}

static int erase_nor(void *context, uint32_t address)
{
    (void)context;
    if (!NOR_READY || (address != SET_A && address != SET_B)) return 0;
    ((void (*)(uint32_t))0x01E50000u)(address >> 12);
    return 1;
}

static enum tsp_result save(const struct h9_api *loader, const struct ts_settings *next)
{
    struct tsp_io io = {
        .ctx = (void *)loader, .read = read_nor, .program = write_nor, .erase = erase_nor,
        .sector = { SET_A, SET_B }
    };
    enum tsp_result status = tsp_check_game_directory(&io, GAME_DIRECTORY);
    if (status != TSP_OK) return status;
    io.ownership_approved = 1;
    return tsp_save(&io, next);
}

int aprs_select_tx(struct ts_settings *s)
{
    unsigned selector = TX_SETTING;
    if (selector > 4) return 0;
    unsigned side = selector == 4 ? ACTIVE_SIDE : selector & 1u;
    uint32_t frequency = *(const volatile uint32_t *)(uintptr_t)(0x11CA4u + side * 8u);
    if (frequency < 14400000u || frequency > 14800000u) return 0;
    s->tx_setting = (uint8_t)selector;
    s->tx_side = (uint8_t)side;
    s->tx_frequency = frequency;
    return 1;
}

static const uint8_t intervals[] = { 1, 2, 5, 10, 15, 30 };
static const char *const symbols[] = { "PERSON", "BIKE", "CAR" };

#ifdef H9_GROUPED_MENU
const char *aprs_setting_value(enum aprs_setting setting, const struct ts_settings *cfg, char minutes[8])
{
    if (!cfg) return "--";
    switch (setting) {
    case APRS_SCHEDULE: return cfg->armed ? "ARMED" : "OFF";
    case APRS_MODE: return cfg->timed ? "TIMED" : "SMART";
    case APRS_INTERVAL:
        if (!cfg->timed) return "AUTO";
        minutes[0] = (char)('0' + cfg->minutes / 10);
        minutes[1] = (char)('0' + cfg->minutes % 10);
        minutes[2] = ' ';
        minutes[3] = 'M';
        minutes[4] = 'I';
        minutes[5] = 'N';
        minutes[6] = 0;
        return minutes + (minutes[0] == '0');
    case APRS_SYMBOL: return symbols[cfg->symbol];
    case APRS_MUTE: return cfg->mute ? "ON" : "OFF";
    default: return "";
    }
}

const char *aprs_setting_adjust(const struct h9_runtime_api *api, enum aprs_setting setting,
                               struct ts_settings *cfg, int direction)
{
    switch (setting) {
    case APRS_SCHEDULE:
        if (!cfg->armed) {
            struct h9_runtime_status status;
            if (!api->read_status(&status) || !status.worker_ready) return "SCHEDULER UNAVAILABLE";
            if (!aprs_select_tx(cfg)) return "CHECK APRS TX CHANNEL";
        }
        cfg->armed ^= 1;
        break;
    case APRS_MODE: cfg->timed ^= 1; break;
    case APRS_INTERVAL: {
        unsigned i = sizeof intervals;
        if (direction > 0) {
            for (i = 0; i < sizeof intervals && intervals[i] <= cfg->minutes; ++i) {}
            if (i == sizeof intervals) i = 0;
        } else {
            while (i && intervals[i - 1] >= cfg->minutes) --i;
            i = i ? i - 1 : sizeof intervals - 1;
        }
        cfg->minutes = intervals[i];
        break;
    }
    case APRS_SYMBOL: cfg->symbol = (cfg->symbol + (direction > 0 ? 1u : 2u)) % 3u; break;
    case APRS_MUTE: cfg->mute ^= 1; break;
    default: break;
    }
    return 0;
}

int aprs_settings_save(const struct h9_api *loader, const struct h9_runtime_api *api,
                       const struct ts_settings *cfg, struct ts_settings *saved)
{
    if (cfg->mute == saved->mute && cfg->armed == saved->armed && cfg->timed == saved->timed &&
        cfg->minutes == saved->minutes && cfg->symbol == saved->symbol &&
        cfg->tx_frequency == saved->tx_frequency && cfg->tx_setting == saved->tx_setting &&
        cfg->tx_side == saved->tx_side) return 1;
    if (save(loader, cfg) != TSP_OK) return 0;
    api->publish_settings(cfg);
    *saved = *cfg;
    return 1;
}
#else
/* The API 4 bench build runs the editor in its APRS BEACON app. */
enum { RUN, MODE, EVERY, SYMBOL, MUTE, ROW_COUNT };
#define TICK (*(const volatile uint32_t *)0x1D8B0u)

static const char *const rows[] = { "SCHEDULE", "MODE", "EVERY", "SYMBOL", "APRS MUTE" };

static void line(unsigned y, const char *s, unsigned color)
{
    gfx_fill(0, y, H9_WIDTH, 10, UI_BG);
    gfx_text(4, y, s, 25, 1, (uint16_t)color, UI_BG);
}

static void draw(const struct h9_runtime_status *s, unsigned selected, const char *notice)
{
    const struct ts_settings *cfg = &s->settings;
    const struct h9_bs_status *beacon = &s->beacon;
    char minutes[] = { (char)('0' + cfg->minutes / 10), (char)('0' + cfg->minutes % 10),
                      ' ', 'M', 'I', 'N', 0 };
    const char *values[] = { cfg->armed ? "ARMED" : "OFF", cfg->timed ? "TIMED" : "SMART",
        cfg->timed ? minutes + (minutes[0] == '0') : "AUTO", symbols[cfg->symbol],
        cfg->mute ? "ON" : "OFF" };
    for (unsigned i = 0; i < ROW_COUNT; ++i) {
        uint16_t fg = i == selected ? UI_BG : UI_TEXT;
        uint16_t bg = i == selected ? UI_ACCENT : UI_BG;
        unsigned y = 21 + i * 13;
        gfx_fill(0, y, H9_WIDTH, 12, bg);
        gfx_text(4, y + 2, rows[i], 12, 1, fg, bg);
        gfx_text(104, y + 2, values[i], 9, 1, fg, bg);
    }
    const char *state = !s->worker_ready || s->polls == 0 ? "SCHEDULER UNAVAILABLE" :
        !s->uart_online ? "GPS LINK WAIT" :
        !beacon->gps.fix ? "WAITING FOR GPS" :
        !beacon->armed ? "GPS FIX" :
        beacon->pending ? "GPS FIX  BEACON DUE" : "GPS FIX  SCHEDULING";
    line(87, state, beacon->gps.fix ? UI_OK : UI_ACCENT);
    line(98, notice ? notice : "BENCH MODE  TX DISABLED", notice ? UI_ACCENT : UI_DIM);
    line(116, selected == RUN ? "HOLD MENU  BACK" : "MENU SET  BACK", UI_TEXT);
}

unsigned module_main(const struct h9_api *loader, unsigned self)
{
    (void)self;
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
    ui_title("SMARTBEACON");
    const struct h9_runtime_api *service =
        loader->version >= 4 && loader->resident ? loader->resident->api : 0;
    if (!service || service->abi != H9_RUNTIME_ABI) {
        line(30, "SCHEDULER UNAVAILABLE", UI_ACCENT);
        gfx_wait_release();
        gfx_wait_key();
        return 0;
    }
    gfx_wait_release();
    struct h9_runtime_status latest;
    unsigned selected = RUN, previous = 0, held = 0;
    uint32_t last_draw = TICK - 100u;
    const char *notice = 0;
    for (;;) {
        unsigned key = h9_key();
        held = key == previous ? held + (held < 100) : 0;
        previous = key;
        if (held == 2 && key == H9_KEY_BACK) return 0;
        if (held == 2 && (key == H9_KEY_UP || key == H9_KEY_DOWN)) {
            selected = (selected + (key == H9_KEY_UP ? ROW_COUNT - 1 : 1)) % ROW_COUNT;
            notice = 0;
            last_draw -= 100u;
        }
        if (service->read_status(&latest)) {
            if (key == H9_KEY_MENU && held == (selected == RUN ? 35u : 2u)) {
                struct ts_settings next = latest.settings;
                int acceptable = 1;
                switch (selected) {
                case RUN:
                    next.armed ^= 1;
                    acceptable = !next.armed || (latest.worker_ready && aprs_select_tx(&next));
                    break;
                case MODE: next.timed ^= 1; break;
                case EVERY: {
                    unsigned i = 0;
                    while (i < sizeof intervals && intervals[i] <= next.minutes) ++i;
                    next.minutes = intervals[i % sizeof intervals];
                    break;
                }
                case SYMBOL: next.symbol = (next.symbol + 1u) % 3; break;
                case MUTE: next.mute ^= 1; break;
                }
                if (!acceptable) notice = "CHECK APRS TX CHANNEL";
                else if (save(loader, &next) != TSP_OK) notice = "SETTINGS SAVE FAILED";
                else {
                    service->publish_settings(&next);
                    notice = "SETTINGS SAVED";
                }
                last_draw -= 100u;
            }
            if (TICK - last_draw >= 100u) {
                draw(&latest, selected, notice);
                last_draw = TICK;
            }
        }
        h9_delay(20);
    }
}
#endif
