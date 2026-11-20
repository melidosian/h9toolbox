#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "h9mod.h"
#include "radio_settings.h"

static uint32_t tick, frequency, vfo_a, vfo_b, gpio_out, gpio_dir;
static uint8_t band, bt_spk, side;
#define RADIO_SCAN_HOST
#define SCAN_TICK tick
#define SCAN_FREQ frequency
#define SCAN_VFO_A vfo_a
#define SCAN_VFO_B vfo_b
#define SCAN_SIDE side
#define SCAN_BAND band
#define SCAN_BT_SPK bt_spk
#define SCAN_GPIO_OUT gpio_out
#define SCAN_GPIO_DIR gpio_dir
#include "radio_scan.c"
#include "radio_tones.h"

struct press { unsigned at, key; };
static const struct press *keys;
static uint16_t registers[128], original_regs[128];
static uint8_t image[0x6000];
static unsigned rf_writes, retunes, writes, fail_on_write, save_counter, sample_tone, saw_spectrum, saw_tone;
static unsigned captured_frequency, captured_code, counter_samples;

static int read_image(unsigned offset, void *out, unsigned length)
{ assert(offset + length <= sizeof image); memcpy(out, image + offset, length); return 0; }
static int write_image(unsigned offset, const void *data, unsigned length)
{
    ++writes;
    if (writes == fail_on_write) return -1;
    assert(offset + length <= sizeof image); memcpy(image + offset, data, length); return 0;
}
const struct radio_storage radio_stock_storage = {read_image, write_image};

unsigned radio_scan_read_reg(unsigned reg)
{
    assert(reg < 128);
    if (reg == 0x67) return frequency == 14552500 ? 240 : 100 + (frequency % 23);
    if (reg == 0x0d && save_counter) {
        ++counter_samples;
        return captured_frequency >> 16;
    }
    if (reg == 0x0e && save_counter) return captured_frequency & 0xffff;
    if (reg == 0x69 && sample_tone) return captured_code & 0x8000 ? radio_scan_dcs_word(captured_code) >> 12 : 0x8000;
    if (reg == 0x6a && sample_tone) return radio_scan_dcs_word(captured_code) & 0xfff;
    if (reg == 0x68 && sample_tone) return captured_code & 0x8000 ? 0x8000 : 183; /* 88.5 Hz */
    return registers[reg];
}
void radio_scan_write_reg(unsigned reg, unsigned value)
{ assert(reg < 128); registers[reg] = value; ++rf_writes; }
void radio_scan_retune(void)
{ ++retunes; registers[0x30] = 0xbdf1; }
void radio_scan_filter(unsigned f)
{ assert(f >= MIN_RX && f <= MAX_RX); }
void radio_scan_internal_spk(unsigned enabled)
{ bt_spk = enabled; }
unsigned h9_key(void)
{
    for (const struct press *p = keys; p->at; ++p) if (tick >= p->at && tick < p->at + 5) return p->key;
    return 0;
}
void h9_delay(unsigned ms) { (void)ms; ++tick; assert(tick < 1600); }
void gfx_wait_release(void) { }
void gfx_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t color)
{ (void)color; assert(x + w <= 160 && y + h <= 128); }
unsigned gfx_text(unsigned x, unsigned y, const char *text, unsigned max, unsigned scale, uint16_t fg, uint16_t bg)
{
    (void)fg; (void)bg;
    unsigned n = 0; while (n < max && text[n]) ++n;
    assert(x + n * 6 * scale <= 160 && y + 8 * scale <= 128);
    saw_spectrum |= !strcmp(text, "SPECTRUM");
    saw_tone |= !strcmp(text, "88.5 Hz");
    return x + n * 6 * scale;
}

static void reset(const struct press *sequence)
{
    keys = sequence; tick = 0; frequency = vfo_a = 14552500; vfo_b = 43512500;
    band = 0; side = 0; bt_spk = 0; gpio_out = 0x400; gpio_dir = 0xffff;
    rf_writes = retunes = writes = fail_on_write = save_counter = sample_tone = 0;
    captured_frequency = 14652000; captured_code = 0x885; counter_samples = 0;
    memset(image, 0xa5, sizeof image);
    image[0x1f2b] = 0xe7; image[0x1f2c] = 3; /* 999 MHz, clamped */
    image[0x1f2d] = image[0x1f2e] = 0;
    for (unsigned i = 0; i < 128; ++i) registers[i] = 0x0100 + i;
    registers[0x30] = 0xbdf1; registers[0x68] = registers[0x69] = registers[0x6a] = 0xffff;
    registers[0x0d] = 0x8000;
    memcpy(original_regs, registers, sizeof registers);
}

static void restored(void)
{
    assert(frequency == 14552500 && !band && !bt_spk && gpio_out == 0x400 && gpio_dir == 0xffff);
    for (unsigned i = 0; i < sizeof preserved_regs; ++i)
        assert(registers[preserved_regs[i]] == original_regs[preserved_regs[i]]);
}

static int same_dcs_stream(unsigned a, unsigned b)
{
    unsigned wa = radio_scan_dcs_word(a), wb = radio_scan_dcs_word(b);
    for (unsigned i = 0; i < 23; ++i) {
        if (wa == wb) return 1;
        wa = (wa >> 1) | ((wa & 1) << 22);
    }
    return 0;
}

int main(void)
{
    assert(radio_scan_counter(0x8000, 0) == 0);
    assert(radio_scan_counter(14652000 >> 16, 14652000 & 0xffff) == 14652000);
    assert(!radio_scan_counter(1, 0));
    assert(radio_scan_next(100, 100, 200, 5, -1) == 200);
    assert(radio_scan_next(200, 100, 200, 5, 1) == 100);
    assert(radio_scan_next(80, 100, 200, 5, 1) == 100);
    assert(radio_scan_ctcss(0x8000) == NONE && radio_scan_dcs(0x8000, 0) == NONE);
    assert(radio_scan_dcs_word(0x8023) == 0x763813); /* pinned stock 0x58ede/0x58ef6, D023N */
    assert(radio_scan_dcs_word(0xc023) == (0x763813 ^ 0x7fffff));
    for (unsigned i = 1; i < sizeof radio_tones / sizeof *radio_tones; ++i) {
        unsigned code = radio_tones[i].value;
        if (code & 0x8000) {
            unsigned word = radio_scan_dcs_word(code), canonical = NONE;
            for (unsigned phase = 0; phase < 23; ++phase) {
                unsigned decoded = radio_scan_dcs(word >> 12, word & 0xfff);
                assert(decoded != NONE && same_dcs_stream(decoded, code));
                if (!phase) canonical = decoded;
                else assert(decoded == canonical);
                word = (word >> 1) | ((word & 1) << 22);
            }
        } else {
            unsigned n = (code & 15) + ((code >> 4) & 15) * 10 + ((code >> 8) & 15) * 100 + ((code >> 12) & 15) * 1000;
            unsigned raw = (n * 2064 + 5000) / 10000;
            assert(radio_scan_ctcss(raw) == code);
        }
    }
    const struct press cancel[] = {{90,H9_KEY_BACK},{0,0}};
    reset(cancel);
    assert(radio_scan(RADIO_SCAN_FREQUENCY) == 0 && !writes && retunes > 0);
    restored();
    const struct press tone[] = {{95,H9_KEY_MENU},{0,0}};
    reset(tone); sample_tone = 1;
    assert(radio_scan(RADIO_SCAN_TONE) == -1 && saw_tone && writes == 2);
    assert(image[0x1958] == 0x85 && image[0x1959] == 8 && image[0x195a] == 0x85 && image[0x195b] == 8);
    assert((image[0xca2] & 3) == 0);
    restored();
    reset(tone); sample_tone = 1;
    const uint8_t rx[] = {0x00,0x25,0x55,0x14}, repeater_tx[] = {0x00,0x25,0x61,0x14};
    memcpy(image + 0x1950, rx, 4); memcpy(image + 0x1954, repeater_tx, 4);
    image[0x195e] = 0xa5; /* plus offset: finding a tone must preserve it */
    assert(radio_scan(RADIO_SCAN_TONE) == -1);
    assert(!memcmp(image + 0x1950, rx, 4) && !memcmp(image + 0x1954, repeater_tx, 4));
    assert(image[0x195e] == 0xa5); restored();
    const struct press found[] = {{100,H9_KEY_MENU},{0,0}};
    reset(found); save_counter = 1;
    assert(radio_scan(RADIO_SCAN_FREQUENCY) == -1 && counter_samples >= 2 && writes == 2);
    const uint8_t encoded[] = {0x00,0x20,0x65,0x14};
    assert(!memcmp(image + 0x1950, encoded, 4) && !memcmp(image + 0x1954, encoded, 4));
    assert(image[0x1958] == 255 && image[0x1959] == 255);
    restored();
    reset(found); save_counter = 1; fail_on_write = 2;
    assert(radio_scan(RADIO_SCAN_FREQUENCY) == RADIO_IO_ERROR);
    for (unsigned i = 0; i < 16; ++i) assert(image[0x1950+i] == 0xa5);
    assert(image[0xca2] == 0xa5); restored();
    const struct press sweep_cancel[] = {{110,H9_KEY_BACK},{0,0}};
    reset(sweep_cancel);
    assert(radio_scan(RADIO_SCAN_SPECTRUM) == 0 && saw_spectrum && retunes > 80 && !writes);
    restored();
    const struct press listen_cancel[] = {{90,H9_KEY_MENU},{110,H9_KEY_BACK},{130,H9_KEY_BACK},{0,0}};
    reset(listen_cancel);
    assert(radio_scan(RADIO_SCAN_SPECTRUM) == 0 && !writes);
    restored();
    const struct press listen_use[] = {{90,H9_KEY_MENU},{110,H9_KEY_MENU},{0,0}};
    reset(listen_use);
    assert(radio_scan(RADIO_SCAN_SPECTRUM) == -1 && writes == 2);
    assert(!(image[0xca2] & 3) && !(image[0x195e] & 3));
    assert(image[0x1958] == 255 && image[0x1959] == 255);
    restored();
    reset(cancel); registers[0x30] |= TX_ON;
    assert(radio_scan(RADIO_SCAN_SPECTRUM) == RADIO_CONFLICT && !rf_writes && !retunes && !writes);
    puts("Scan: all CTCSS/DCS codes and phases, counter capture, spectrum, cancellation, audio restore and rollback passed.");
}
