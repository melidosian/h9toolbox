#include <stdint.h>

/* RAM immediately above the stock stack is inside the stock clear range.
 * A debugger or readback-capable monitor can inspect these words. */
#define BOOT_PROBE_WORD ((volatile uint32_t *)(uintptr_t)0x0000E2E0u)
#define BOOT_PROBE_INVERSE ((volatile uint32_t *)(uintptr_t)0x0000E2E4u)
#define BOOT_PROBE_MAGIC 0x48394254u /* "H9BT" in big-endian notation */
#define LED9_BANK ((volatile uint32_t *)(uintptr_t)0x001E5000u)
#define LED9_MASK 0x200u
#define GPIO_SYNC ((volatile uint32_t *)(uintptr_t)0x001E0200u)

extern unsigned char __stock_clear_start[];
extern unsigned char __stock_clear_end[];

/* Stock v1.0.38: app+0x088686 configures pin 9; app+0x05036A drives
 * bank 0 bit 9 for the breathing LED. The boot-time GPIO clock state and
 * visible timing still require a physical check. */
__attribute__((noreturn)) void h9_main_led9_blink(void)
{
    volatile uint32_t *bank = LED9_BANK;
    uint32_t pulse;

    (void)*GPIO_SYNC;
    bank[0] &= ~LED9_MASK;
    bank[3] |= LED9_MASK;
    bank[2] &= ~LED9_MASK;

    for (;;) {
        for (pulse = 0; pulse < 3u; ++pulse) {
            volatile uint32_t delay = 2000000u;
            (void)*GPIO_SYNC;
            bank[0] |= LED9_MASK;
            while (delay-- != 0u) { }
            (void)*GPIO_SYNC;
            bank[0] &= ~LED9_MASK;
            delay = 2000000u;
            while (delay-- != 0u) { }
        }
        {
            volatile uint32_t delay = 8000000u;
            while (delay-- != 0u) { }
        }
    }
}

__attribute__((noreturn)) void h9_main_runtime_init(void)
{
    volatile unsigned char *cursor = __stock_clear_start;
    while (cursor < __stock_clear_end) *cursor++ = 0;

    *BOOT_PROBE_WORD = BOOT_PROBE_MAGIC;
    *BOOT_PROBE_INVERSE = ~BOOT_PROBE_MAGIC;
    h9_main_led9_blink();
}
