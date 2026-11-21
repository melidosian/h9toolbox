/* Pinned factory v1.0.38 backends; none enters the retired menu renderer. */
#include "radio_system.h"

int radio_system_reload_native(void);
int radio_system_factory_native(unsigned action);
int radio_system_header_native(unsigned area, void *dst);
void radio_system_uart_native(const void *, unsigned length);

static void text(char *dst, const char *src, unsigned capacity)
{
    unsigned i = 0;
    while (i + 1 < capacity && src[i] >= 32 && src[i] <= 126) { dst[i] = src[i]; ++i; }
    dst[i] = 0;
}
static void identity(struct radio_system_identity *id)
{
    id->region = *(const volatile uint8_t *)(0x11AD0u + 4300);
    text(id->base, (const char *)0x01EB19BBu, sizeof id->base);
    uint8_t header[32] = {0};
    text(id->hardware, "UNKNOWN", sizeof id->hardware);
    if (!radio_system_header_native(0, header) && header[2] >= 32 && header[2] <= 126 &&
        header[3] >= 32 && header[3] <= 126 && header[4] >= 32 && header[4] <= 126) {
        id->hardware[0] = 'V'; id->hardware[1] = header[2]; id->hardware[2] = '.';
        id->hardware[3] = header[3]; id->hardware[4] = '.'; id->hardware[5] = header[4]; id->hardware[6] = 0;
    }
    uint8_t ani[3];
    static const char tones[] = "0123456789ABCD*#";
    if (radio_stock_storage.read(0x1820, ani, 3)) text(id->ani, "---", sizeof id->ani);
    else {
        for (unsigned i = 0; i < 3; ++i) id->ani[i] = ani[i] < 16 ? tones[ani[i]] : '-';
        id->ani[3] = 0;
    }
}
static int ready(void) { return *(const volatile uint16_t *)0x12234u == 0x5AA5u; }
static int reload(void)
{
    if (!ready()) return RADIO_IO_ERROR;
    return radio_system_reload_native();
}
static int reset(unsigned action)
{
    if (!ready()) return RADIO_IO_ERROR;
    if (action > 4) return RADIO_INVALID;
    int result = radio_system_factory_native(action);
    if (result) return result;
    /* Analog reset copies exactly 912 bytes from the factory table. Verify
     * those writes rather than trusting the old void-returning handler. */
    if (action == 2 || action == 3) {
        const uint8_t *defaults = (const uint8_t *)0x01EAF348u;
        uint8_t check[16];
        for (unsigned at = 0; at < 912; at += sizeof check) {
            if (radio_stock_storage.read(0x5000 + at, check, sizeof check)) return RADIO_IO_ERROR;
            for (unsigned i = 0; i < sizeof check; ++i) if (check[i] != defaults[at+i]) return RADIO_VERIFY_ERROR;
        }
    }
    return RADIO_OK;
}
static void query(void)
{
    if (!*(const volatile uint32_t *)0x11CFCu) return;
    uint8_t request[] = {0xc0,0xc0,0xc0,0xc0,0xc0,0x20,0,0xc0,0xc0,0xc0};
    radio_system_uart_native(request, sizeof request);
}
static unsigned ring_end(void) { return *(const volatile uint16_t *)0x16E34u & 2047; }
static unsigned ring_byte(unsigned at) { return ((const volatile uint8_t *)0x16634u)[at & 2047]; }
static unsigned ticks(void) { return *(const volatile uint32_t *)0x1D8B0u; }
const struct radio_system_platform radio_system_platform = { identity, reload, reset, query, ring_end, ring_byte, ticks };
