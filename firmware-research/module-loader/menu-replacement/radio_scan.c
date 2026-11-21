/* Stock v1.0.38 RX primitives with Toolbox-owned counter, tone and scope UI.
 * No stock menu/display callbacks are called. The caller holds the resident
 * transmitter's stock_tx reservation for this complete synchronous action. */
#include "radio_scan.h"
#include "radio_settings.h"
#include "app_theme.h"

unsigned radio_scan_read_reg(unsigned reg);
void radio_scan_write_reg(unsigned reg, unsigned value);
void radio_scan_retune(void);
void radio_scan_filter(unsigned frequency);
void radio_scan_internal_spk(unsigned enabled);

#ifndef RADIO_SCAN_HOST
#define SCAN_TICK (*(const volatile uint32_t *)0x1d8b0u)
#define SCAN_FREQ (*(volatile uint32_t *)(0x11ad0u + 444))
#define SCAN_VFO_A (*(const volatile uint32_t *)(0x11ad0u + 464))
#define SCAN_VFO_B (*(const volatile uint32_t *)(0x11ad0u + 472))
#define SCAN_SIDE (*(const volatile uint8_t *)(0x11ad0u + 128))
#define SCAN_BAND (*(volatile uint8_t *)(0x11ad0u + 4301))
#define SCAN_BT_SPK (*(const volatile uint8_t *)(0xc0f0u + 204))
#define SCAN_GPIO_OUT (*(volatile uint32_t *)0x1e5000u)
#define SCAN_GPIO_DIR (*(volatile uint32_t *)0x1e5008u)
#endif

enum { NONE = 0xffff, BINS = 80, TX_ON = 0x4000, MIN_RX = 1800000, MAX_RX = 60000000 };

/* Same supported tones as dist/channel.mjs, in tenths of a Hz / packed octal.
 * Kept compact here instead of duplicating the channel editor's label table. */
static const uint16_t ctcss[] = {
    670,693,719,744,770,797,825,854,885,915,948,974,1000,1035,1072,1109,1148,
    1188,1230,1273,1318,1365,1413,1462,1514,1567,1598,1622,1655,1679,1713,
    1738,1773,1799,1835,1862,1899,1928,1966,1995,2035,2065,2107,2181,2257,
    2291,2336,2418,2503,2541
};
static const uint16_t dcs[] = {
    0x023,0x025,0x026,0x031,0x032,0x036,0x043,0x047,0x051,0x053,0x054,0x065,
    0x071,0x072,0x073,0x074,0x114,0x115,0x116,0x122,0x125,0x131,0x132,0x134,
    0x143,0x145,0x152,0x155,0x156,0x162,0x165,0x172,0x174,0x205,0x212,0x223,
    0x225,0x226,0x243,0x244,0x245,0x246,0x251,0x252,0x255,0x261,0x263,0x265,
    0x266,0x271,0x274,0x306,0x311,0x315,0x325,0x331,0x332,0x343,0x346,0x351,
    0x356,0x364,0x365,0x371,0x411,0x412,0x413,0x423,0x431,0x432,0x445,0x446,
    0x452,0x454,0x455,0x462,0x464,0x465,0x466,0x503,0x506,0x516,0x523,0x526,
    0x532,0x546,0x565,0x606,0x612,0x624,0x627,0x631,0x632,0x654,0x662,0x664,
    0x703,0x712,0x723,0x731,0x732,0x734,0x743,0x754
};

static unsigned bcd(unsigned value)
{
    unsigned result = 0, shift = 0;
    do { result |= value % 10 << shift; value /= 10; shift += 4; } while (value);
    return result;
}

unsigned radio_scan_ctcss(unsigned raw)
{
    if (raw & 0x8000) return NONE;
    unsigned tone10 = (raw & 0x1fff) * 10000u / 2064u;
    unsigned best = NONE, error = 6;
    for (unsigned i = 0; i < sizeof ctcss / sizeof *ctcss; ++i) {
        unsigned delta = tone10 > ctcss[i] ? tone10 - ctcss[i] : ctcss[i] - tone10;
        if (delta < error) { error = delta; best = bcd(ctcss[i]); }
    }
    return best;
}

unsigned radio_scan_dcs_word(unsigned code)
{
    /* app+0x58ede maps packed octal digits into the 12-bit DCS data word.
     * app+0x58ef6 computes its eleven parity bits using feedback 0x8ea. */
    unsigned data = (code & 15) | ((code >> 1) & 0x78) | ((code >> 2) & 0x3c0) | 0x800;
    unsigned parity = data;
    for (unsigned i = 0; i < 12; ++i) {
        parity <<= 1;
        if (parity & 0x1000) parity ^= 0x8ea;
    }
    unsigned word = data | ((parity >> 1) & 0x7ff) << 12;
    return code & 0x4000 ? word ^ 0x7fffff : word;
}

unsigned radio_scan_dcs(unsigned high, unsigned low)
{
    if (high & 0x8000) return NONE;
    unsigned original = ((high & 0x7ff) << 12) | (low & 0xfff);
    if (!original || original == 0x7fffff) return NONE;
    unsigned best = NONE;
    for (unsigned reverse = 0; reverse < 2; ++reverse) {
        unsigned word = reverse ? original ^ 0x7fffff : original;
        for (unsigned rotate = 0; rotate < 23; ++rotate) {
            if ((word & 0xe00) == 0x800) {
                for (unsigned i = 0; i < sizeof dcs / sizeof *dcs; ++i)
                    if (radio_scan_dcs_word(dcs[i]) == word) {
                        unsigned code = dcs[i] | (reverse ? 0xc000 : 0x8000);
                        if (code < best) best = code;
                    }
            }
            word = (word >> 1) | ((word & 1) << 22);
        }
    }
    return best;
}

unsigned radio_scan_counter(unsigned high, unsigned low)
{
    if (high & 0x8000) return 0;
    unsigned frequency = ((high & 0x7ff) << 16) | (low & 0xffff);
    return frequency >= MIN_RX && frequency <= MAX_RX ? frequency : 0;
}

uint32_t radio_scan_next(uint32_t frequency, uint32_t low, uint32_t high, uint32_t step, int direction)
{
    if (low > high || !step) return low;
    if (frequency < low || frequency > high) return direction < 0 ? high : low;
    return direction < 0 ? (frequency - low >= step ? frequency - step : high)
        : (high - frequency >= step ? frequency + step : low);
}

static char *append(char *out, const char *text)
{ while (*text) *out++ = *text++; *out = 0; return out; }

static char *number(char *out, unsigned n, unsigned width)
{
    char reverse[10]; unsigned count = 0;
    do { reverse[count++] = '0' + n % 10; n /= 10; } while (n);
    while (count < width) reverse[count++] = '0';
    while (count) *out++ = reverse[--count];
    *out = 0; return out;
}

static char *frequency_label(char *out, unsigned frequency)
{ out = number(out, frequency / 100000, 1); *out++ = '.'; return number(out, frequency % 100000, 5); }

static void tone_label(char out[16], unsigned tone)
{
    if (tone == NONE) { append(out, "SEARCHING TONE"); return; }
    if (tone & 0x8000) {
        out[0] = 'D'; out[1] = '0' + ((tone >> 8) & 7); out[2] = '0' + ((tone >> 4) & 7);
        out[3] = '0' + (tone & 7); out[4] = tone & 0x4000 ? 'R' : 'N'; out[5] = 0;
    } else {
        unsigned n = ((tone >> 12) & 15) * 1000 + ((tone >> 8) & 15) * 100 +
                     ((tone >> 4) & 15) * 10 + (tone & 15);
        char *p = number(out, n / 10, 1); *p++ = '.'; *p++ = '0' + n % 10; append(p, " Hz");
    }
}

static unsigned signal_level(void)
{
    unsigned sum = 0;
    for (unsigned i = 0; i < 5; ++i) sum += radio_scan_read_reg(0x67) & 0x1ff;
    return sum / 5;
}

static void tune(unsigned frequency)
{
    SCAN_FREQ = frequency;
    SCAN_BAND = frequency > 29999999;
    radio_scan_retune();
}

static const uint8_t preserved_regs[] = { 0x30,0x31,0x32,0x37,0x47,0x48,0x50,0x51,0x58,0x70 };
struct saved_radio {
    uint32_t frequency, gpio_out, gpio_dir;
    uint16_t regs[sizeof preserved_regs];
    uint8_t band, bt_spk, side;
};

static void capture(struct saved_radio *saved)
{
    saved->frequency = SCAN_FREQ; saved->band = SCAN_BAND; saved->bt_spk = SCAN_BT_SPK;
    saved->gpio_out = SCAN_GPIO_OUT & 8; saved->gpio_dir = SCAN_GPIO_DIR & 8;
    saved->side = SCAN_FREQ == SCAN_VFO_B && SCAN_FREQ != SCAN_VFO_A ? 1 :
        SCAN_FREQ == SCAN_VFO_A && SCAN_FREQ != SCAN_VFO_B ? 0 : SCAN_SIDE >> 7;
    for (unsigned i = 0; i < sizeof preserved_regs; ++i) saved->regs[i] = radio_scan_read_reg(preserved_regs[i]);
}

static void restore(const struct saved_radio *saved)
{
    radio_scan_write_reg(0x32, 0x244); /* stop the frequency counter before restoring synth state */
    tune(saved->frequency); SCAN_BAND = saved->band;
    for (unsigned i = 0; i < sizeof preserved_regs; ++i) radio_scan_write_reg(preserved_regs[i], saved->regs[i]);
    radio_scan_internal_spk(saved->bt_spk);
    SCAN_GPIO_OUT = (SCAN_GPIO_OUT & ~8u) | saved->gpio_out;
    SCAN_GPIO_DIR = (SCAN_GPIO_DIR & ~8u) | saved->gpio_dir;
}

static void listen(unsigned frequency)
{
    tune(frequency);
    radio_scan_write_reg(0x47, 0x6140); /* native AF mode 1, app+0x592e4 */
    radio_scan_write_reg(0x30, 0xbff5); /* native receive-open value, app+0x83a42 */
    radio_scan_internal_spk(1);
    SCAN_GPIO_DIR &= ~8u; SCAN_GPIO_OUT |= 8u;
}

static void limits(unsigned *low, unsigned *high)
{
    uint8_t bytes[4]; *low = MIN_RX; *high = MAX_RX;
    if (radio_stock_storage.read(0x1f2b, bytes, 4)) return;
    unsigned upper = (bytes[0] | bytes[1] << 8) * 100000u;
    unsigned lower = (bytes[2] | bytes[3] << 8) * 100000u;
    if (lower < MIN_RX) lower = MIN_RX;
    if (upper > MAX_RX) upper = MAX_RX;
    if (lower < upper) { *low = lower; *high = upper; }
}

static int write_checked(unsigned offset, const uint8_t *data, unsigned length)
{
    uint8_t verify[16];
    if (radio_stock_storage.write(offset, data, length) || radio_stock_storage.read(offset, verify, length))
        return RADIO_IO_ERROR;
    for (unsigned i = 0; i < length; ++i) if (verify[i] != data[i]) return RADIO_VERIFY_ERROR;
    return RADIO_OK;
}

static int save_result(unsigned side, unsigned frequency, unsigned tone, unsigned tone_only)
{
    unsigned offset = side ? 0x1960 : 0x1950, mode_offset = side ? 0xca3 : 0xca2;
    uint8_t original[16], next[16], mode, new_mode;
    if (radio_stock_storage.read(offset, original, 16) || radio_stock_storage.read(mode_offset, &mode, 1)) return RADIO_IO_ERROR;
    for (unsigned i = 0; i < 16; ++i) next[i] = original[i];
    if (frequency < MIN_RX || frequency > MAX_RX) return RADIO_INVALID;
    unsigned old_frequency = 0, place = 1;
    for (unsigned i = 0; i < 4; ++i) {
        unsigned pair = original[i];
        if ((pair & 15) > 9 || (pair >> 4) > 9) { old_frequency = 0; break; }
        old_frequency += ((pair & 15) + (pair >> 4) * 10) * place; place *= 100;
    }
    if (!tone_only || old_frequency != frequency) {
        unsigned n = frequency;
        for (unsigned i = 0; i < 4; ++i) { unsigned pair = n % 100; next[i] = next[4+i] = pair % 10 | pair / 10 << 4; n /= 100; }
        next[14] &= ~3u; /* simplex, as the native captured-frequency path */
    }
    if (tone != NONE || !tone_only) {
        next[8] = next[10] = tone; next[9] = next[11] = tone >> 8;
    }
    int result = write_checked(offset, next, 16);
    new_mode = mode & ~3u;
    if (!result) result = write_checked(mode_offset, &new_mode, 1);
    if (result) {
        /* A failed publication must not leave a partially edited VFO selected. */
        int rollback = write_checked(offset, original, 16);
        if (!rollback) rollback = write_checked(mode_offset, &mode, 1);
        return rollback ? RADIO_VERIFY_ERROR : result;
    }
    return -1;
}

struct key_state { unsigned sample, stable; };
static unsigned key_press(struct key_state *state)
{
    unsigned key = h9_key(), pressed = 0;
    if (key == state->sample && key != state->stable) {
        state->stable = key; pressed = key;
    }
    state->sample = key; return pressed;
}

static unsigned observed_tone(void)
{
    (void)radio_scan_read_reg(0x0c); /* native consumes the detector interrupt latch */
    unsigned high = radio_scan_read_reg(0x69);
    unsigned tone = radio_scan_dcs(high, radio_scan_read_reg(0x6a));
    return tone != NONE ? tone : radio_scan_ctcss(radio_scan_read_reg(0x68));
}

static void begin_tone(void)
{
    radio_scan_write_reg(0x51, 0x0300);
    radio_scan_write_reg(0x37, 0x1f0f);
    radio_scan_write_reg(0x30, 0);
    radio_scan_write_reg(0x30, 0xbdf1);
}

static void show_counter(unsigned mode, unsigned frequency, unsigned tone, unsigned rssi, unsigned side)
{
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
    ui_title(mode == RADIO_SCAN_TONE ? "SUB-AUDIO FINDER" : "FREQUENCY CAPTURE");
    char text[32], *p;
    if (frequency) { p = frequency_label(text, frequency); append(p, " MHz"); }
    else append(text, "SEARCHING SIGNAL");
    gfx_text(3, 27, text, 26, 1, UI_TEXT, UI_BG);
    tone_label(text, tone);
    gfx_text(3, 45, text, 26, 1, tone != NONE ? UI_OK : UI_HINT, UI_BG);
    p = append(text, "RSSI ");
    int dbm = (int)(rssi >> 1) - 160;
    if (dbm < 0) { *p++ = '-'; dbm = -dbm; }
    p = number(p, dbm, 1); append(p, " dBm");
    gfx_text(3, 65, text, 26, 1, UI_TEXT, UI_BG);
    append(text, side ? "USE RESULT IN VFO B" : "USE RESULT IN VFO A");
    gfx_text(3, 90, text, 26, 1, UI_DIM, UI_BG);
    ui_footer("MENU USE 0 RESCAN BACK");
}

static int finder(unsigned mode, const struct saved_radio *saved, unsigned *out_freq, unsigned *out_tone)
{
    struct key_state key = {0};
    unsigned low, high; limits(&low, &high);
    unsigned frequency = mode == RADIO_SCAN_TONE ? saved->frequency : 0;
    unsigned previous_frequency = 0, previous_tone = NONE, tone = NONE, ready = 0, alternate = 0;
    uint32_t next = SCAN_TICK + 25u, filter_at = SCAN_TICK, drawn = SCAN_TICK - 20u;
    tune(saved->frequency);
    if (mode == RADIO_SCAN_TONE) begin_tone();
    else {
        radio_scan_write_reg(0x70, 0); radio_scan_write_reg(0x58, 0);
        radio_scan_filter(low); radio_scan_write_reg(0x32, 0x245);
    }
    for (;;) {
        h9_delay(2);
        unsigned pressed = key_press(&key);
        if (pressed == H9_KEY_BACK) return RADIO_OK;
        if (radio_scan_read_reg(0x30) & TX_ON) return RADIO_CONFLICT;
        if (pressed == H9_KEY_MENU && frequency && (mode != RADIO_SCAN_TONE || tone != NONE)) {
            *out_freq = frequency; *out_tone = tone; return -1;
        }
        if (pressed == 2) { /* digit 0: resume the same finder */
            tone = previous_tone = NONE; previous_frequency = ready = 0;
            frequency = mode == RADIO_SCAN_TONE ? saved->frequency : 0;
            if (frequency) begin_tone();
            else radio_scan_write_reg(0x32, 0x245);
            next = SCAN_TICK + 25u;
        }
        if ((int32_t)(SCAN_TICK - next) >= 0) {
            next = SCAN_TICK + 10u;
            if (!frequency) {
                unsigned upper = radio_scan_read_reg(0x0d);
                if (!(upper & 0x8000)) {
                    unsigned candidate = radio_scan_counter(upper, radio_scan_read_reg(0x0e));
                    radio_scan_write_reg(0x32, 0x244);
                    if (candidate >= low && candidate <= high) {
                        unsigned delta = candidate > previous_frequency ? candidate - previous_frequency : previous_frequency - candidate;
                        if (previous_frequency && delta <= 10) {
                            frequency = candidate; tune(frequency); begin_tone(); next = SCAN_TICK + 25u;
                        }
                        previous_frequency = candidate;
                    } else previous_frequency = 0;
                    if (!frequency) radio_scan_write_reg(0x32, 0x245);
                }
                if (high >= 24500000 && low < 24500000 && SCAN_TICK - filter_at >= 150u) {
                    alternate ^= 1; filter_at = SCAN_TICK;
                    radio_scan_filter(alternate ? 30000000 : low);
                }
            } else if (tone == NONE) {
                unsigned candidate = observed_tone();
                if (candidate != NONE && candidate == previous_tone) ++ready;
                else { previous_tone = candidate; ready = 0; }
                if (ready) tone = candidate;
            }
        }
        if (SCAN_TICK - drawn >= 20u) {
            show_counter(mode, frequency, tone, signal_level(), saved->side); drawn = SCAN_TICK;
        }
    }
}

static void spectrum_bounds(unsigned centre, unsigned span, unsigned low, unsigned high, unsigned *start, unsigned *step)
{
    if (span > high - low) span = high - low;
    unsigned first = centre > span / 2 ? centre - span / 2 : low;
    if (first < low) first = low;
    if (first > high - span) first = high - span;
    *step = span / (BINS - 1); if (!*step) *step = 1;
    *start = first;
}

static void show_spectrum(const uint16_t levels[BINS], const uint16_t peaks[BINS], unsigned start, unsigned step,
                          unsigned peak, unsigned listening, unsigned span)
{
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
    ui_title(listening ? "SPECTRUM LISTEN" : "SPECTRUM");
    unsigned floor = 511, top = 0;
    for (unsigned i = 0; i < BINS; ++i) { if (levels[i] < floor) floor = levels[i]; if (peaks[i] > top) top = peaks[i]; }
    if (top - floor < 40) top = floor + 40;
    for (unsigned i = 0; i < BINS; ++i) {
        unsigned h = levels[i] > floor ? (levels[i] - floor) * 50 / (top - floor) : 0;
        unsigned ph = peaks[i] > floor ? (peaks[i] - floor) * 50 / (top - floor) : 0;
        if (h > 50) h = 50;
        if (ph > 50) ph = 50;
        if (h) gfx_fill(i * 2, 91 - h, 1, h, i == peak ? UI_ACCENT : UI_OK);
        gfx_fill(i * 2, 91 - ph, 1, 1, UI_HINT);
    }
    char text[32], *p = frequency_label(text, start + peak * step); append(p, " MHz");
    gfx_text(3, 23, text, 26, 1, UI_TEXT, UI_BG);
    p = append(text, "SPAN "); p = number(p, span / 100000, 1); append(p, " MHz");
    gfx_text(3, 101, text, 26, 1, UI_DIM, UI_BG);
    ui_footer(listening ? "MENU USE VFO BACK SCOPE" : "MENU LISTEN UP/DN SPAN BACK");
}

static int spectrum(const struct saved_radio *saved, unsigned *out_frequency)
{
    static const uint32_t spans[] = { 200000,300000,400000,600000 };
    uint16_t levels[BINS] = {0}, peaks[BINS] = {0};
    struct key_state key = {0};
    unsigned low, high; limits(&low, &high);
    unsigned centre = saved->frequency, span_index = 2, at = 0, best = 0, listening = 0;
    unsigned start, step; spectrum_bounds(centre, spans[span_index], low, high, &start, &step);
    uint32_t settled = SCAN_TICK + 1u;
    tune(start);
    show_spectrum(levels, peaks, start, step, best, 0, spans[span_index]);
    for (;;) {
        h9_delay(2);
        unsigned pressed = key_press(&key);
        if (radio_scan_read_reg(0x30) & TX_ON) return RADIO_CONFLICT;
        if (pressed == H9_KEY_BACK) {
            if (!listening) return RADIO_OK;
            listening = 0; tune(start + at * step); settled = SCAN_TICK + 1u;
        } else if (pressed == H9_KEY_MENU) {
            if (listening) { *out_frequency = start + best * step; return -1; }
            listening = 1; listen(start + best * step);
            show_spectrum(levels, peaks, start, step, best, 1, spans[span_index]);
        } else if (pressed == H9_KEY_UP || pressed == H9_KEY_DOWN) {
            span_index = (span_index + (pressed == H9_KEY_UP ? 1 : 3)) % 4;
            spectrum_bounds(centre, spans[span_index], low, high, &start, &step);
            for (unsigned i = 0; i < BINS; ++i) levels[i] = peaks[i] = 0;
            at = best = listening = 0; tune(start); settled = SCAN_TICK + 1u;
        } else if (pressed == H9_KEY_1 || pressed == H9_KEY_3) {
            centre = radio_scan_next(centre, low, high, spans[span_index] / 2, pressed == H9_KEY_1 ? -1 : 1);
            spectrum_bounds(centre, spans[span_index], low, high, &start, &step);
            for (unsigned i = 0; i < BINS; ++i) levels[i] = peaks[i] = 0;
            at = best = listening = 0; tune(start); settled = SCAN_TICK + 1u;
        }
        if (!listening && (int32_t)(SCAN_TICK - settled) >= 0) {
            levels[at] = signal_level();
            if (levels[at] >= peaks[at]) peaks[at] = levels[at];
            else if (peaks[at]) --peaks[at];
            if (levels[at] > levels[best]) best = at;
            ++at;
            if (at == BINS) { show_spectrum(levels, peaks, start, step, best, 0, spans[span_index]); at = 0; }
            tune(start + at * step); settled = SCAN_TICK + 1u;
        }
    }
}

int radio_scan(unsigned mode)
{
    if (mode > RADIO_SCAN_SPECTRUM) return RADIO_INVALID;
    if (radio_scan_read_reg(0x30) & TX_ON) return RADIO_CONFLICT;
    struct saved_radio saved; capture(&saved);
    if (saved.frequency < MIN_RX || saved.frequency > MAX_RX) return RADIO_INVALID;
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
    ui_title(mode == RADIO_SCAN_SPECTRUM ? "SPECTRUM" : "SCAN");
    gfx_wait_release();
    unsigned frequency = 0, tone = NONE;
    int result = mode == RADIO_SCAN_SPECTRUM ? spectrum(&saved, &frequency) : finder(mode, &saved, &frequency, &tone);
    /* An asynchronous external transmit owns the chip until its TX bit drops.
     * Do not restore receive registers over a live PTT/KISS transmission. */
    while (radio_scan_read_reg(0x30) & TX_ON) h9_delay(10);
    restore(&saved);
    gfx_wait_release();
    if (result == -1) result = save_result(saved.side, frequency, tone, mode == RADIO_SCAN_TONE);
    return result;
}
