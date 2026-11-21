/* TX runs synchronously in the stock my_app timed-beacon context.
 * The native selector/preparation/unkey sequence owns the RF hardware.
 * Only the checked frequency boundary and the APRS payload are overridden. */
#include "beacon_tx.h"
#include "../modules/aprs/aprs_tx.h"
#include <stddef.h>

#ifndef H9_TX_BYTE
#define H9_TX_BYTE(addr) (*(volatile uint8_t *)(uintptr_t)(addr))
#define H9_TX_WORD(addr) (*(const volatile uint32_t *)(uintptr_t)(addr))
#define LOCK_IRQ() ((void (*)(void))0x02001634u)()
#define UNLOCK_IRQ() ((void (*)(void))0x02001650u)()
#define RF_READ(reg) ((unsigned (*)(unsigned))0x01E505FEu)(reg)
#define RF_SETUP() ((void (*)(unsigned))0x01E58E3Eu)(0)
#define RF_REDRAW() ((void (*)(unsigned))0x01E53306u)(0)
#define RF_RECEIVE() ((void (*)(void))0x01E595B6u)()
#define RX_SETTLE() ((void (*)(unsigned))0x02001F82u)(5)
#define STOCK_BEACON() ((unsigned (*)(void))0x01E5C3D4u)()
#define STOCK_PREPARE(p) ((unsigned (*)(const void *))0x01E5AD68u)(p)
#define STOCK_END() ((void (*)(unsigned))0x01E5CDECu)(0)
#define STOCK_EMIT() ((unsigned (*)(void))0x01E5BF2Cu)()
#define STOCK_AIRTIME() ((unsigned (*)(void))0x01E5C36Cu)()
#define STOCK_TX_HZ(f, shift) ((uint32_t (*)(uint32_t, unsigned))0x01E56AE2u)((f), (shift))
#define FRAME_MODEM(raw, length, wire, capacity) \
    ((unsigned (*)(unsigned, const uint8_t *, unsigned, uint8_t *, unsigned))0x01E5B42Eu)(0, raw, length, wire, capacity)
#define WRITE_MODEM(wire, length) ((void (*)(const uint8_t *, unsigned))0x01E5B4AAu)(wire, length)
#endif

#define TX_FLAG    H9_TX_BYTE(0x11D28u)
#define SIDE_BYTE  H9_TX_BYTE(0x11B50u)
#define ROUTE_BYTE H9_TX_BYTE(0x11AECu)
#define TX_SETTING H9_TX_BYTE(0xC533u)

/* BK4819 register 0x0C bit 1 is the live squelch output. Bit 0 is the IRQ line. */
enum { RF_TX = 0x4000, RF_CARRIER = 1u << 1, WIRE_MAX = 2 * APRS_TX_FRAME_MAX + 10 };

struct h9_beacon_attempt {
    const struct ts_settings *config;
    uint32_t rx;
    unsigned wire_length, airtime;
    unsigned refusal;
    uint8_t entered, keyed, submitted;
    uint8_t wire[WIRE_MAX];
};

struct stock_frequency {
    uint32_t tx, rx;
    uint8_t shift;
};
_Static_assert(offsetof(struct stock_frequency, shift) == 8, "Stock TX argument layout");

static unsigned modem_ready(void)
{
    /* UART1 serial object and its live TX callback. */
    return H9_TX_WORD(0x11CFCu) == 0x1E32Cu && H9_TX_WORD(0x1E340u) != 0;
}

static unsigned normal_idle(void)
{
    return H9_TX_BYTE(0x11AB0u) <= 1 && H9_TX_BYTE(0x11AC6u) == 3 &&
           !H9_TX_BYTE(0x11AFBu) && !H9_TX_BYTE(0x11AFEu) &&
           !H9_TX_BYTE(H9_RF_SLEEP_FLAG) && !H9_TX_BYTE(0xC3AFu);
}

static unsigned uses_stock_aprs(void)
{
    /* Protect the other autonomous TX sources while this request owns RF. */
    return H9_TX_BYTE(0xC4A0u) != 1 || H9_TX_BYTE(0xC512u) || H9_TX_BYTE(0xC513u) ||
           H9_TX_BYTE(0xC517u) || H9_TX_BYTE(0xC520u);
}

static unsigned stock_position_ready(void)
{
    /* Decode at app+0x5C2D8–0x5C30C. Both coordinate words may not be zero. */
    return H9_TX_BYTE(0xC1CAu) != 1 ||
           (H9_TX_BYTE(0x12550u) && (H9_TX_WORD(0x12558u) || H9_TX_WORD(0x12560u)));
}

static unsigned rf_idle(void)
{
    return normal_idle() && !(RF_READ(0x30) & RF_TX) && !(RF_READ(0x0c) & RF_CARRIER);
}

static uint32_t vfo_word(unsigned side, unsigned tx)
{
    return H9_TX_WORD(0x11CA0u + 8u * side + 4u * tx);
}

static unsigned vfo_refusal(const struct ts_settings *cfg)
{
    unsigned selector = TX_SETTING, side = cfg->tx_side;
    if (selector > 4 || cfg->tx_setting != selector || side > 1 ||
        (selector < 4 && (selector & 1u) != side)) return H9_BS_SETTING;
    /* The stock Lock and current-channel modes wait for the selected VFO.
     * The Auto modes may borrow the configured VFO while the radio is idle. */
    if ((selector < 2 || selector == 4) && (SIDE_BYTE >> 7) != side)
        return H9_BS_SIDE;
    if (cfg->tx_frequency < TX_LOW || cfg->tx_frequency > TX_HIGH ||
        vfo_word(side, 1) != cfg->tx_frequency) return H9_BS_FREQUENCY;
    return 0;
}

static unsigned working_route(const struct h9_beacon_attempt *a)
{
    const struct ts_settings *cfg = a->config;
    return TX_SETTING == cfg->tx_setting && (SIDE_BYTE >> 7) == cfg->tx_side &&
           ROUTE_BYTE == cfg->tx_side &&
           H9_TX_WORD(0x11C8Cu) == a->rx && H9_TX_WORD(0x11C9Cu) == cfg->tx_frequency &&
           vfo_word(cfg->tx_side, 0) == a->rx && vfo_word(cfg->tx_side, 1) == cfg->tx_frequency;
}

static unsigned make_packet(struct h9_beacon_attempt *a, const struct h9_bs_status *beacon)
{
    static const char symbols[] = { '[', 'b', '>' };
    char call[10], info[BEACON_INFO];
    uint8_t frame[APRS_TX_FRAME_MAX];
    unsigned n = 0;
    while (n < 6) {
        unsigned c = H9_TX_BYTE(0xC4A1u + n);
        if (!c || c == ' ' || c == 0xff) break;
        call[n++] = (char)c;
    }
    unsigned ssid = H9_TX_BYTE(0xC4A7u);
    if (!n || ssid > 15) return H9_BS_CALLSIGN;
    if (ssid) {
        call[n++] = '-';
        if (ssid >= 10) { call[n++] = '1'; ssid -= 10; }
        call[n++] = (char)('0' + ssid);
    }
    call[n] = 0;
    if (beacon->symbol >= sizeof symbols) return H9_BS_SETTING;
    beacon_info(info, &beacon->gps, symbols[beacon->symbol], "H9 Toolbox");
    unsigned length = aprs_tx_frame(frame, call, APRS_TX_DEST, APRS_TX_PATH, info);
    if (!length) return H9_BS_CALLSIGN;
    a->wire_length = FRAME_MODEM(frame, length, a->wire, sizeof a->wire);
    if (!a->wire_length || a->wire_length > sizeof a->wire) return H9_BS_TX_ERROR;
    /* Stock allows 800 ms for modem startup/tail. Use the actual frame size
     * including FCS and allow worst-case HDLC stuffing at 1200 baud. */
    a->airtime = 800u + 8u * (length + 2u);
    return 0;
}

static unsigned acquire(struct h9_beacon_tx *tx)
{
    /* Brief critical section shared with AF01 and remote PTT. Neither the
     * stock operations nor the RF packet wait holds interrupts off. */
    LOCK_IRQ();
    unsigned free = !tx->active && !tx->stock_calls;
    if (free) tx->active = 1;
    UNLOCK_IRQ();
    return free;
}

static void select_side(unsigned side, unsigned route)
{
    SIDE_BYTE = (uint8_t)((SIDE_BYTE & 0x7fu) | side << 7);
    ROUTE_BYTE = (uint8_t)route;
}

void h9_beacon_tx_stock(struct h9_beacon_tx *tx, const void *arg, unsigned length, h9_stock_action stock)
{
    LOCK_IRQ();
    unsigned allowed = !tx->active && tx->stock_calls != UINT16_MAX;
    if (allowed) ++tx->stock_calls;
    UNLOCK_IRQ();
    if (!allowed) return;
    stock(arg, length);
    LOCK_IRQ();
    --tx->stock_calls;
    UNLOCK_IRQ();
}

unsigned h9_beacon_tx_prepare(struct h9_beacon_tx *tx, const void *frequency)
{
    struct h9_beacon_attempt *a = tx->attempt;
    if (!a) return STOCK_PREPARE(frequency);
    a->entered = 1;
    unsigned why = vfo_refusal(a->config);
    if (!why && !working_route(a)) why = H9_BS_SIDE;
    const struct stock_frequency *f = frequency;
    if (!why && (!f || f->tx != a->config->tx_frequency || f->rx != a->rx ||
                 STOCK_TX_HZ(H9_TX_BYTE(0x12BBAu) ? f->rx : f->tx, f->shift) != a->config->tx_frequency))
        why = H9_BS_FREQUENCY;
    if (!why && uses_stock_aprs()) why = H9_BS_STOCK_CONTROL;
    if (!why && (!rf_idle() || TX_FLAG != 1 || !modem_ready()))
        why = H9_BS_BUSY;
    if (why) { a->refusal = why; return 0; }
    unsigned result = STOCK_PREPARE(frequency);
    a->keyed = result != 0;
    if (!a->keyed) a->refusal = H9_BS_TX_ERROR;
    return result;
}

unsigned h9_beacon_tx_emit(struct h9_beacon_tx *tx)
{
    struct h9_beacon_attempt *a = tx->attempt;
    if (!a) return STOCK_EMIT();
    if (!a->keyed || a->submitted) return 0;
    if (TX_FLAG != 1 || !(RF_READ(0x30) & RF_TX) || !working_route(a) ||
        !modem_ready() || uses_stock_aprs()) {
        a->refusal = H9_BS_TX_ERROR;
        return 0;
    }
    WRITE_MODEM(a->wire, a->wire_length);
    a->submitted = 1;
    return 1;
}

unsigned h9_beacon_tx_airtime(struct h9_beacon_tx *tx)
{
    return tx->attempt ? tx->attempt->airtime : STOCK_AIRTIME();
}

unsigned h9_beacon_tx_try(struct h9_beacon_tx *tx, const struct h9_runtime_status *request)
{
    if (tx->fault) return H9_BS_TX_FAULT;
    if (!request->beacon.gps.fix || !request->beacon.gps.lat[0]) return H9_BS_POSITION;
    unsigned why = vfo_refusal(&request->settings);
    if (why) return why;
    if (uses_stock_aprs()) return H9_BS_STOCK_CONTROL;
    if (!stock_position_ready()) return H9_BS_STOCK_POSITION;
    if (!normal_idle() || TX_FLAG || tx->stock_calls || !modem_ready()) return H9_BS_BUSY;

    struct h9_beacon_attempt a = { .config = &request->settings,
                                   .rx = vfo_word(request->settings.tx_side, 0) };
    why = make_packet(&a, &request->beacon);
    if (why) return why;
    if (!acquire(tx)) return H9_BS_BUSY;
    why = vfo_refusal(a.config);
    if (!why && (TX_FLAG || ROUTE_BYTE > 1 || !rf_idle())) why = H9_BS_BUSY;
    if (!why && uses_stock_aprs()) why = H9_BS_STOCK_CONTROL;
    if (!why) {
        unsigned side = SIDE_BYTE >> 7, route = ROUTE_BYTE;
        unsigned borrowed = !working_route(&a);
        /* Start the native selector already on its target. Its alternate
         * path otherwise restores the prior side before the caller unkeys. */
        select_side(a.config->tx_side, a.config->tx_side);
        if (borrowed) {
            RF_SETUP();
            RF_RECEIVE();
            RX_SETTLE();
        }
        if (!working_route(&a)) why = H9_BS_SIDE;
        else if (TX_FLAG || !rf_idle()) why = H9_BS_BUSY;
        else {
            tx->attempt = &a;
            STOCK_BEACON();
            tx->attempt = NULL;
            if (a.entered) STOCK_END();
            why = a.submitted ? H9_TX_SUBMITTED : a.refusal;
            if (!a.entered) why = H9_BS_STOCK_POSITION;
            else if (!a.submitted && !why) why = H9_BS_TX_ERROR;
        }
        if (TX_FLAG || (RF_READ(0x30) & RF_TX)) {
            tx->fault = 1;
            why = H9_BS_TX_FAULT;
        } else {
            if ((SIDE_BYTE >> 7) != side || ROUTE_BYTE != route) {
                select_side(side, route);
                RF_SETUP();
                RF_RECEIVE();
            }
            RF_REDRAW();
        }
    }
    /* Atomic byte release after the final hardware and route checks. */
    tx->active = 0;
    return why;
}
