#include "gfx.h"
#include "h9mod.h"
#include "../../resident-service/runtime.h"

enum { SET_A = 0x3FC000, SET_B = 0x3FD000, GAME_DIRECTORY = 0xA1F000 };
enum { RUN, MODE, EVERY, SYMBOL, MUTE, ROW_COUNT };
#define NOR_READY (*(const volatile uint16_t *)0x12234u == 0x5AA5u)
#define TX_SETTING (*(const volatile uint8_t *)0xC533u)
#define ACTIVE_SIDE (*(const volatile uint8_t *)0x11B50u >> 7)
#define TICK (*(const volatile uint32_t *)0x1D8B0u)

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

static int select_tx(struct ts_settings *s)
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
static const char *const rows[] = { "SCHEDULE", "MODE", "EVERY", "SYMBOL", "APRS MUTE" };

static void line(unsigned y, const char *s, unsigned color)
{
    gfx_fill(0, y, H9_WIDTH, 10, GFX_BLACK);
    gfx_text(4, y, s, 25, 1, (uint16_t)color, GFX_BLACK);
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
        uint16_t fg = i == selected ? GFX_BLACK : GFX_WHITE;
        uint16_t bg = i == selected ? GFX_YELLOW : GFX_BLACK;
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
    line(87, state, beacon->gps.fix ? GFX_GREEN : GFX_YELLOW);
    line(98, notice ? notice : "BENCH MODE  TX DISABLED", notice ? GFX_YELLOW : GFX_GREY);
    line(116, selected == RUN ? "HOLD MENU  BACK" : "MENU SET  BACK", GFX_WHITE);
}

unsigned module_main(const struct h9_api *loader, unsigned self)
{
    (void)self;
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, GFX_BLACK);
    gfx_fill(0, 0, H9_WIDTH, 18, GFX_BLUE);
    gfx_text(4, 1, "SMARTBEACON", 12, 2, GFX_WHITE, GFX_BLUE);
    const struct h9_runtime_api *service =
        loader->version >= 4 && loader->resident ? loader->resident->api : 0;
    if (!service || service->abi != H9_RUNTIME_ABI) {
        line(30, "SCHEDULER UNAVAILABLE", GFX_YELLOW);
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
                    acceptable = !next.armed || (latest.worker_ready && select_tx(&next));
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
