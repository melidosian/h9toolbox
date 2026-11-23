#include <stddef.h>
#include "capture.h"

enum {
    ADC_CLOCK = 0x1d8b0,
    ADC_CACHE = 0x12a30,
    ADC_CURSOR = 0x11b39,
    TX_STATE = 0x11d28,
    LOG_RAM = 0x2b700,
    LAST_TICK_RAM = 0x2be00,
    SAMPLE_TICKS = 4,
    CLONE_BASE = 0x3c00,
};

static volatile struct swr_bench *const log = (volatile struct swr_bench *)LOG_RAM;
static volatile uint32_t *const last_tick = (volatile uint32_t *)LAST_TICK_RAM;

_Static_assert(sizeof(struct swr_bench) <= LAST_TICK_RAM - LOG_RAM, "bench RAM layout");

struct adc_cache_slot {
    uint32_t id;
    uint16_t value;
    uint16_t padding;
};

_Static_assert(sizeof(struct adc_cache_slot) == 8, "stock ADC cache slot");
_Static_assert(offsetof(struct adc_cache_slot, value) == 4, "stock ADC sample offset");

static void tick(void *context)
{
    ((void (*)(void *))0x01e896c8u)(context);

    if (log->status >= SWR_BENCH_COMPLETE) return;
    uint32_t now = *(const volatile uint32_t *)ADC_CLOCK;
    if (now - *last_tick < SAMPLE_TICKS) return;
    *last_tick = now;

    const volatile struct adc_cache_slot *cache = (const volatile struct adc_cache_slot *)ADC_CACHE;
    uint16_t values[SWR_BENCH_ADC_COUNT] = {
        SWR_BENCH_MISSING, SWR_BENCH_MISSING, SWR_BENCH_MISSING,
        SWR_BENCH_MISSING, SWR_BENCH_MISSING,
    };
    for (unsigned i = 0; i < 10; ++i) {
        switch (cache[i].id) {
        case 0: values[0] = cache[i].value; break;
        case 3: values[1] = cache[i].value; break;
        case 4: values[2] = cache[i].value; break;
        case 10: values[3] = cache[i].value; break;
        case 13: values[4] = cache[i].value; break;
        }
    }
    swr_bench_sample(log, now, *(const volatile uint8_t *)TX_STATE,
                     *(const volatile uint8_t *)ADC_CURSOR, values);
}

unsigned swr_bench_register(void *context, void (*original)(void *), unsigned interval, unsigned mode)
{
    swr_bench_init(log);
    *last_tick = *(const volatile uint32_t *)ADC_CLOCK - SAMPLE_TICKS;
    /* The build pins the original callback address at the registration site. */
    (void)original;
    return ((unsigned (*)(void *, void (*)(void *), unsigned, unsigned))0x02003904u)
        (context, tick, interval, mode);
}

int swr_bench_read(unsigned partition, unsigned offset, uint8_t *byte, unsigned length)
{
    unsigned index = offset - CLONE_BASE;
    if (partition != 4 || length != 1 || index >= SWR_BENCH_WIRE_SIZE)
        return ((int (*)(unsigned, unsigned, uint8_t *, unsigned))0x01e52cb2u)
            (partition, offset, byte, length);

    const volatile uint8_t *wire = (const volatile uint8_t *)LOG_RAM;
    /* Stock clone calls this byte by byte. Publish samples only after the
     * timer has finished the capture and stopped mutating the wire image. */
    *byte = 0;
    if (log->magic == SWR_BENCH_MAGIC &&
        (index < offsetof(struct swr_bench, records) || log->status >= SWR_BENCH_COMPLETE))
        *byte = wire[index];
    return 0;
}
