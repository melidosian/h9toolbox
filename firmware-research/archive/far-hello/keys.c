/* Far keys: the hold-8 recovery test. Runs from main flash 0xEA000 (Back-hold
 * launch on H9 Toolbox Firmware with the far launcher), so the radio has
 * booted normally and stock hold-8 is unaffected whatever this does.
 *
 * Shows, live:
 *   KEY   the key byte at RAM 0xC3AF (what h9_key() returns)
 *   HOLD  the key-state byte at RAM 0xC39D (stock's boot-key handler
 *         checks it for 14)
 *   USB   seconds left while KEY reads 5
 * Stock's boot-key handler (app+0x7D418) calls ROM 0x02000D68 when the key
 * byte is 5, after drawing "USB Update". If KEY reads 5 for 2 s here, this
 * program does the same: if the Mac then sees 4C4A:2342 (BR23UBOOT1.00), our
 * own firmware can enter the mask-ROM USB loader without stock my_app.
 *
 * Freestanding: no globals, no library calls (the build checks). */
#include <stdint.h>

void h9_lcd_window(unsigned x0, unsigned y0, unsigned x1, unsigned y1);
void h9_lcd_write(const void *buffer, unsigned length);
unsigned h9_key(void);
void h9_delay(unsigned units);
void h9_led(unsigned on);
void h9_enter_usb_loader(void); /* ROM 0x02000D68; does not return */

enum { WIDTH = 160, HEIGHT = 128, CHUNK = 128 };
enum { USB_KEY = 5, USB_TICKS = 200 }; /* 2 s of 10 ms ticks */
enum { WHITE = 0xffff, BLACK = 0x0000, YELLOW = 0xffe0, GREEN = 0x07e0 };

#ifdef H9_HOST_TEST
uint32_t h9_host_ticks(void);
unsigned h9_host_byte(unsigned address);
static uint32_t ticks(void) { return h9_host_ticks(); }
static unsigned ram_byte(unsigned address) { return h9_host_byte(address); }
#else
static uint32_t ticks(void) { return *(volatile uint32_t *)0x1d8b0; }
static unsigned ram_byte(unsigned address) { return *(volatile uint8_t *)address; }
#endif

static void fill(unsigned x0, unsigned y0, unsigned w, unsigned h, uint16_t color)
{
    uint8_t buffer[CHUNK];
    for (unsigned i = 0; i < CHUNK; i += 2) {
        buffer[i] = (uint8_t)(color >> 8);
        buffer[i + 1] = (uint8_t)color;
    }
    h9_lcd_window(x0, y0, x0 + w - 1, y0 + h - 1);
    for (unsigned left = w * h * 2; left; ) {
        unsigned n = left < CHUNK ? left : CHUNK;
        h9_lcd_write(buffer, n);
        left -= n;
    }
}

/* 3 x 5 font, rows top to bottom, 3 bits each (MSB = left). */
static unsigned glyph(char c)
{
    static const char chars[] = "0123456789ABDEHKLMOPRSUWY";
    static const uint16_t bits[] = {
        075557, 026227, 071747, 071717, 055711, 074717, 074757, 071111, 075757, 075717,
        025755, 065656, 065556, 074647, 055755, 055655, 044447, 057755, 075557, 075744,
        065655, 074717, 055557, 055775, 055222,
    };
    for (unsigned i = 0; chars[i]; i++)
        if (chars[i] == c) return bits[i];
    return 0;
}

/* Text at 2x scale: 6 x 10 pixels per character, 8 px advance. */
static void text(unsigned x, unsigned y, const char *s, uint16_t fg, uint16_t bg)
{
    for (; *s; s++, x += 8) {
        uint8_t buffer[CHUNK];
        unsigned bits = glyph(*s), used = 0;
        h9_lcd_window(x, y, x + 7, y + 9);
        for (unsigned row = 0; row < 10; row++) {
            for (unsigned col = 0; col < 8; col++) {
                unsigned on = col < 6 && ((bits >> (14 - (row / 2) * 3 - col / 2)) & 1);
                uint16_t pixel = on ? fg : bg;
                buffer[used++] = (uint8_t)(pixel >> 8);
                buffer[used++] = (uint8_t)pixel;
                if (used == CHUNK) { h9_lcd_write(buffer, used); used = 0; }
            }
        }
        if (used) h9_lcd_write(buffer, used);
    }
}

/* "LABEL" followed by a right-aligned unsigned number, 12 characters. */
static void line(unsigned y, const char *label, uint32_t value, uint16_t color)
{
    char s[13];
    unsigned i = 0;
    for (; label[i] && i < 6; i++) s[i] = label[i];
    for (; i < 12; i++) s[i] = ' ';
    s[12] = 0;
    for (i = 11; ; i--) {
        s[i] = (char)('0' + value % 10);
        value /= 10;
        if (!value || i == 6) break;
    }
    text(0, y, s, color, BLACK);
}

void h9_main(void)
{
    fill(0, 0, WIDTH, HEIGHT, BLACK);
    text(0, 100, "HOLD KEY 5 2S", YELLOW, BLACK); /* key byte 5, expected to be the 8 key */
    unsigned shown_key = ~0u, shown_hold = ~0u, shown_left = ~0u, held = 0, led = 0;
    uint32_t since = 0;
    for (;;) {
        unsigned key = h9_key();
        unsigned hold = ram_byte(0xc39d);
        uint32_t now = ticks();
        if (key != shown_key) {
            shown_key = key;
            line(0, "KEY", key, WHITE);
            led = !led;
            h9_led(led);
        }
        if (hold != shown_hold) {
            shown_hold = hold;
            line(12, "HOLD", hold, WHITE);
        }
        if (key == USB_KEY) {
            if (!held) { held = 1; since = now; }
            uint32_t elapsed = now - since;
            unsigned left = elapsed >= USB_TICKS ? 0 : (USB_TICKS - elapsed + 99) / 100;
            if (left != shown_left) { shown_left = left; line(36, "USB", left, GREEN); }
            if (elapsed >= USB_TICKS) {
                fill(0, 0, WIDTH, HEIGHT, BLACK);
                text(16, 56, "USB", GREEN, BLACK);
                h9_delay(1000); /* as stock does before the ROM call */
                h9_enter_usb_loader();
            }
        } else if (held) {
            held = 0;
            shown_left = ~0u;
            fill(0, 36, WIDTH, 10, BLACK);
        }
        h9_delay(10);
    }
}
