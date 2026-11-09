/* Trap-test monitor: replaces stock my_app (task entry redirected), as the
 * Doom firmware will. Runs from main flash 0xEA000.
 *
 * 1. Recovery first: if the 3 key (key byte 5) is held at power-on, enter
 *    the mask-ROM USB loader (BR23UBOOT1.00), the same mode hold-8 gives.
 * 2. Show the last trap record left in RAM (0x1E4F0: "TRAP" + stub address),
 *    if any, then clear it.
 * 3. Run: uptime and a heartbeat. Any code that jumps into the trapped
 *    my_app-only block lands on a stub that records its address and enters
 *    the USB loader, so a trap shows up on the Mac as 4C4A:2342.
 * Holding 3 for 2 s at any time also enters the USB loader (for reflashing).
 *
 * Freestanding: no globals, no library calls (the build checks). */
#include <stdint.h>

void h9_lcd_window(unsigned x0, unsigned y0, unsigned x1, unsigned y1);
void h9_lcd_write(const void *buffer, unsigned length);
unsigned h9_key(void);
void h9_delay(unsigned units);
void h9_led(unsigned on);
void h9_enter_usb_loader(void);

enum { WIDTH = 160, HEIGHT = 128, CHUNK = 128 };
enum { RECOVERY_KEY = 5, HOLD_TICKS = 200 };
enum { WHITE = 0xffff, BLACK = 0x0000, YELLOW = 0xffe0, GREEN = 0x07e0, RED = 0xf800 };
#define TRAP_RECORD 0x1e4f0u
#define TRAP_MAGIC 0x50415254u /* "TRAP" */

#ifdef H9_HOST_TEST
uint32_t h9_host_ticks(void);
uint32_t *h9_host_ram(unsigned address);
static uint32_t ticks(void) { return h9_host_ticks(); }
static volatile uint32_t *word(unsigned address) { return h9_host_ram(address); }
#else
static uint32_t ticks(void) { return *(volatile uint32_t *)0x1d8b0; }
static volatile uint32_t *word(unsigned address) { return (volatile uint32_t *)address; }
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

static unsigned glyph(char c)
{
    static const char chars[] = "0123456789ABCDEFHKLMOPRSTUWY";
    static const uint16_t bits[] = {
        075557, 026227, 071747, 071717, 055711, 074717, 074757, 071111, 075757, 075717,
        025755, 065656, 074447, 065556, 074647, 074644, 055755, 055655, 044447, 057755,
        075557, 075744, 065655, 074717, 072222, 055557, 055775, 055222,
    };
    for (unsigned i = 0; chars[i]; i++)
        if (chars[i] == c) return bits[i];
    return 0;
}

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

/* "LABEL" then a right-aligned number (hex when base 16), 12 characters. */
static void line(unsigned y, const char *label, uint32_t value, unsigned base, uint16_t color)
{
    static const char digits[] = "0123456789ABCDEF";
    char s[13];
    unsigned i = 0;
    for (; label[i] && i < 5; i++) s[i] = label[i];
    for (; i < 12; i++) s[i] = ' ';
    s[12] = 0;
    for (i = 11; ; i--) {
        s[i] = digits[value % base];
        value /= base;
        if (!value || i == 5) break;
    }
    text(0, y, s, color, BLACK);
}

static void usb(void)
{
    fill(0, 0, WIDTH, HEIGHT, BLACK);
    text(56, 56, "USB", GREEN, BLACK);
    h9_delay(1000);
    h9_enter_usb_loader();
}

void h9_main(void)
{
    /* 0. Alive: LED on and a blue screen before anything else, so a black
     *    screen with no LED means this code never ran. */
    h9_led(1);
    fill(0, 0, WIDTH, HEIGHT, 0x001f);
    /* 1. Recovery key at power-on, checked for about 0.3 s. */
    for (unsigned i = 0; i < 30; i++) {
        if (h9_key() == RECOVERY_KEY) usb();
        h9_delay(10);
    }
    fill(0, 0, WIDTH, HEIGHT, BLACK);
    text(0, 0, "TRAP TEST", YELLOW, BLACK);
    /* 2. A record from before a reset, if RAM kept it. */
    if (*word(TRAP_RECORD) == TRAP_MAGIC) {
        line(24, "LAST", *word(TRAP_RECORD + 4), 16, RED);
        *word(TRAP_RECORD) = 0;
    } else {
        text(0, 24, "NO TRAP SEEN", GREEN, BLACK);
    }
    text(0, 108, "HOLD 3 USB", WHITE, BLACK);
    /* 3. Run. */
    uint32_t start = ticks(), shown = ~0u, since = 0;
    unsigned held = 0, led = 0;
    for (;;) {
        uint32_t seconds = (ticks() - start) / 100;
        if (seconds != shown) {
            shown = seconds;
            line(48, "UP S", seconds, 10, WHITE);
            led = !led;
            h9_led(led);
        }
        if (h9_key() == RECOVERY_KEY) {
            if (!held) { held = 1; since = ticks(); }
            if (ticks() - since >= HOLD_TICKS) usb();
        } else {
            held = 0;
        }
        h9_delay(10);
    }
}
