#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "beacon_tx.h"

static uint8_t ram[0x20000];
static struct h9_beacon_tx board;
static struct h9_runtime_status request;
static unsigned irq_depth, native_calls, keys, frames, stops, setups, redraws, stock_calls;
static unsigned keyed, wrong_route, wrong_shift, change_after_key, reject_prepare, fail_stop;
static unsigned entered_during_stock, consume_during_beacon;
static unsigned carriers, rf_irq, receiving_side, settles;

static uint32_t word(unsigned address)
{
    uint32_t v;
    memcpy(&v, ram + address, sizeof v);
    return v;
}
static void put_word(unsigned address, uint32_t value)
{
    memcpy(ram + address, &value, sizeof value);
}

static void irq_off(void) { assert(!irq_depth++); }
static void irq_on(void) { assert(irq_depth-- == 1); }
static unsigned stock_read(unsigned reg)
{
    assert(!irq_depth);
    if (reg == 0x30) return keyed ? 0x4000 : 0;
    assert(reg == 0x0c && !keyed);
    return (((carriers >> receiving_side) & 1u) << 1) | rf_irq;
}
static void stock_setup(void);
static void stock_receive(void) { assert(!keyed && !irq_depth); receiving_side = ram[0x11aec]; }
static unsigned stock_beacon(void);
static unsigned stock_prepare(const void *p);
static void stock_end(void);
static unsigned stock_emit(void) { ++stock_calls; return 7; }
static unsigned stock_airtime(void) { ++stock_calls; return 900; }
static unsigned stock_frequency(unsigned tx, unsigned mode) { return mode ? tx + 60000 : tx; }
static unsigned frame_modem(const uint8_t *raw, unsigned n, uint8_t *wire, unsigned capacity);
static void write_modem(const uint8_t *wire, unsigned n);
static void external(const void *payload, unsigned size)
{
    assert(!irq_depth);
    assert(payload != NULL && size == 2);
    ++stock_calls;
    if (entered_during_stock) assert(h9_beacon_tx_try(&board, &request) == H9_BS_BUSY);
}

#define H9_TX_BYTE(addr) ram[(addr)]
#define H9_TX_WORD(addr) word(addr)
#define LOCK_IRQ() irq_off()
#define UNLOCK_IRQ() irq_on()
#define RF_READ(reg) stock_read(reg)
#define RF_SETUP() stock_setup()
#define RF_REDRAW() (++redraws)
#define RF_RECEIVE() stock_receive()
#define RX_SETTLE() (++settles)
#define STOCK_BEACON() stock_beacon()
#define STOCK_PREPARE(p) stock_prepare(p)
#define STOCK_END() stock_end()
#define STOCK_EMIT() stock_emit()
#define STOCK_AIRTIME() stock_airtime()
#define STOCK_TX_HZ(tx, mode) stock_frequency((tx), (mode))
#define FRAME_MODEM(raw, n, wire, capacity) frame_modem((raw), (n), (wire), (capacity))
#define WRITE_MODEM(wire, n) write_modem((wire), (n))
#include "beacon_tx.c"

static void stock_setup(void)
{
    assert(!keyed && !irq_depth);
    unsigned side = ROUTE_BYTE;
    put_word(0x11C8C, vfo_word(side, 0));
    put_word(0x11C9C, vfo_word(side, 1));
    ++setups;
}

static unsigned stock_prepare(const void *p)
{
    const struct stock_frequency *f = p;
    assert(!irq_depth);
    if (reject_prepare) {
        TX_FLAG = 0;
        return 0;
    }
    assert(TX_FLAG == 1 && !keyed);
    assert(f->tx == request.settings.tx_frequency && ROUTE_BYTE == request.settings.tx_side);
    ++keys;
    keyed = 1;
    return 1;
}

static void stock_end(void)
{
    assert(!irq_depth);
    ++stops;
    if (fail_stop) return;
    ROUTE_BYTE = SIDE_BYTE >> 7;
    keyed = TX_FLAG = 0;
    stock_setup();
    stock_receive();
}

static unsigned stock_beacon(void)
{
    assert(board.active && !irq_depth);
    ++native_calls;
    /* Native code needs no side change, so it cannot restore the old side
     * during its post-packet dwell while the transmitter still owns RF. */
    assert((SIDE_BYTE >> 7) == request.settings.tx_side);
    ROUTE_BYTE = SIDE_BYTE >> 7;
    stock_setup();
    if (wrong_route) put_word(0x11C9C, vfo_word(!ROUTE_BYTE, 1));
    TX_FLAG = 1;
    struct stock_frequency f = { word(0x11C9C), word(0x11C8C), (uint8_t)wrong_shift };
    if (consume_during_beacon) h9_beacon_tx_stock(&board, "pk", 2, external);
    h9_beacon_tx_prepare(&board, &f);
    if (change_after_key) ROUTE_BYTE ^= 1;
    unsigned emitted = h9_beacon_tx_emit(&board);
    assert(!h9_beacon_tx_emit(&board));
    if (emitted) assert(h9_beacon_tx_airtime(&board) >= 800 + 8 * 40);
    return emitted;
}

static unsigned frame_modem(const uint8_t *raw, unsigned n, uint8_t *wire, unsigned capacity)
{
    assert(n > 42 && capacity >= 2 * n + 10 && !keyed);
    assert(raw[6] == 0xe0 && raw[13] == 0x7e && raw[20] == 0x63);
    assert(raw[21] == 3 && raw[22] == 0xf0 && raw[23] == '!' && raw[42] == 'b');
    memcpy(wire, raw, n);
    return n;
}

static void write_modem(const uint8_t *wire, unsigned n)
{
    assert(!irq_depth && keyed && TX_FLAG == 1);
    assert(ROUTE_BYTE == request.settings.tx_side && word(0x11C9C) == request.settings.tx_frequency);
    assert(n > 42 && wire[23] == '!');
    ++frames;
}

static void reset(void)
{
    memset(ram, 0, sizeof ram);
    board = (struct h9_beacon_tx){ 0 };
    request = (struct h9_runtime_status){
        .settings = { .armed = 1, .timed = 1, .minutes = 1, .symbol = 1, .tx_side = 1,
                      .tx_setting = 3, .tx_frequency = 14439000 },
        .beacon = { .armed = 1, .pending = 1, .symbol = 1,
                    .gps = { .lat = "4152.68N", .lon = "08737.80W", .fix = 1 } }
    };
    ram[0x11AC6] = 3;
    ram[0x11B50] = 0x39;       /* unrelated logical-side state must survive */
    ram[0xC4A0] = 1;
    memcpy(ram + 0xC4A1, "N0CALL", 6);
    ram[0xC4A7] = 15;
    ram[0xC533] = 3;           /* B-Auto while A is selected */
    put_word(0x11CFC, 0x1E32C);
    put_word(0x1E340, 0x1234);
    put_word(0x11CA0, 44539000); put_word(0x11CA4, 44539000);
    put_word(0x11CA8, 14439000); put_word(0x11CAC, 14439000);
    keyed = wrong_route = wrong_shift = change_after_key = reject_prepare = fail_stop = 0;
    entered_during_stock = consume_during_beacon = 0;
    carriers = rf_irq = receiving_side = settles = 0;
    irq_depth = native_calls = keys = frames = stops = setups = redraws = stock_calls = 0;
    stock_setup();
}

int main(void)
{
    reset();
    rf_irq = 1;  /* An unrelated receiver interrupt does not indicate a carrier. */
    consume_during_beacon = entered_during_stock = 1;
    h9_beacon_tx_stock(&board, "pk", 2, external);
    assert(stock_calls == 1 && !board.stock_calls && !native_calls);
    assert(h9_beacon_tx_try(&board, &request) == H9_TX_SUBMITTED);
    assert(native_calls == 1 && keys == 1 && frames == 1 && stops == 1);
    assert(!keyed && !TX_FLAG && !board.active && !board.stock_calls && stock_calls == 1);
    assert(SIDE_BYTE == 0x39 && ROUTE_BYTE == 0 && word(0x11C9C) == 44539000 &&
           redraws == 1 && receiving_side == 0 && settles == 1);
    assert(h9_beacon_tx_emit(&board) == 7 && h9_beacon_tx_airtime(&board) == 900);

    reset();
    request.settings.tx_setting = TX_SETTING = 1;
    assert(h9_beacon_tx_try(&board, &request) == H9_BS_SIDE && !native_calls);
    SIDE_BYTE |= 0x80;          /* logical B, Dual Watch was still on A */
    assert(h9_beacon_tx_try(&board, &request) == H9_TX_SUBMITTED);
    assert(SIDE_BYTE == 0xb9 && ROUTE_BYTE == 0 && !keyed && frames == 1);

    reset();
    carriers = 1;  /* Current A is receiving, so keep it. */
    assert(h9_beacon_tx_try(&board, &request) == H9_BS_BUSY && !native_calls && !settles);
    carriers = 2;  /* A is clear, but B is busy after tuning to it. */
    assert(h9_beacon_tx_try(&board, &request) == H9_BS_BUSY && !native_calls && settles == 1);
    assert(receiving_side == 0 && SIDE_BYTE == 0x39 && ROUTE_BYTE == 0 && !TX_FLAG);
    carriers = 0;
    assert(h9_beacon_tx_try(&board, &request) == H9_TX_SUBMITTED && frames == 1);

    reset();
    request.settings.timed = 0;
    ram[0xC513] = 1;
    assert(h9_beacon_tx_try(&board, &request) == H9_BS_STOCK_CONTROL && !native_calls);
    ram[0xC513] = 0;
    ram[0x11AFE] = 1;
    assert(h9_beacon_tx_try(&board, &request) == H9_BS_BUSY && !native_calls);
    ram[0x11AFE] = 0;
    request.beacon.gps.fix = 0;
    assert(h9_beacon_tx_try(&board, &request) == H9_BS_POSITION && !native_calls);
    request.beacon.gps.fix = 1;
    ram[0xC1CA] = ram[0x12550] = 1;
    assert(h9_beacon_tx_try(&board, &request) == H9_BS_STOCK_POSITION && !native_calls);
    put_word(0x12560, 1);      /* Latitude can be zero at the equator. */
    put_word(0x11CAC, 14500000);
    assert(h9_beacon_tx_try(&board, &request) == H9_BS_FREQUENCY && !native_calls);
    put_word(0x11CAC, 14439000);
    ram[0xC4A7] = 16;
    assert(h9_beacon_tx_try(&board, &request) == H9_BS_CALLSIGN && !native_calls);
    ram[0xC4A7] = 15;
    wrong_route = 1;
    assert(h9_beacon_tx_try(&board, &request) == H9_BS_SIDE && !keys && !frames && stops == 1);
    wrong_route = 0;
    wrong_shift = 1;
    assert(h9_beacon_tx_try(&board, &request) == H9_BS_FREQUENCY && !keys && !frames && stops == 2);
    wrong_shift = 0;
    reject_prepare = 1;
    assert(h9_beacon_tx_try(&board, &request) == H9_BS_TX_ERROR && !keys && !frames && stops == 3);
    reject_prepare = 0;
    change_after_key = 1;
    assert(h9_beacon_tx_try(&board, &request) == H9_BS_TX_ERROR && keys == 1 && !frames && stops == 4);
    assert(ROUTE_BYTE == 0 && word(0x11C9C) == 44539000);
    change_after_key = 0;
    assert(h9_beacon_tx_try(&board, &request) == H9_TX_SUBMITTED && frames == 1);

    reset();
    fail_stop = 1;
    assert(h9_beacon_tx_try(&board, &request) == H9_BS_TX_FAULT && board.fault);
    assert(ROUTE_BYTE == 1 && keyed);   /* never select A while TX is still indicated */
    assert(h9_beacon_tx_try(&board, &request) == H9_BS_TX_FAULT && frames == 1 && native_calls == 1);
    puts("Beacon TX VFO, stock ownership, frame, retries, and RX restoration PASS (simulated radio)");
}
