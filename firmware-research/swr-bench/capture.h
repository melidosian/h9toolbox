#ifndef H9_SWR_BENCH_CAPTURE_H
#define H9_SWR_BENCH_CAPTURE_H

#include <stddef.h>
#include <stdint.h>

enum {
    SWR_BENCH_VERSION = 1,
    SWR_BENCH_PRE = 16,
    SWR_BENCH_RECORDS = 100,
    SWR_BENCH_POST = 12,
    SWR_BENCH_ADC_COUNT = 5,
    SWR_BENCH_MISSING = 0xffff,
    SWR_BENCH_MAGIC = 0x57533948, /* H9SW, little-endian */
    SWR_BENCH_ARMED = 0,
    SWR_BENCH_CAPTURING = 1,
    SWR_BENCH_COMPLETE = 2,
    SWR_BENCH_LIMIT = 3
};

struct swr_bench_record {
    uint32_t tick;
    uint16_t adc[SWR_BENCH_ADC_COUNT]; /* Stock channel IDs 0, 3, 4, 10, 13. */
    uint8_t tx_flag;
    uint8_t adc_cursor;
};

struct swr_bench {
    uint32_t magic;
    uint8_t version;
    uint8_t status;
    uint8_t count;      /* Number of records from index 16 onward. */
    uint8_t pre_head;   /* Next slot in the pretrigger ring. */
    uint8_t pre_count;
    uint8_t record_size;
    uint8_t reserved[22];
    struct swr_bench_record records[SWR_BENCH_RECORDS];
    uint8_t post_count; /* Private state beyond the export boundary. */
};

enum { SWR_BENCH_WIRE_SIZE = offsetof(struct swr_bench, post_count) };

_Static_assert(offsetof(struct swr_bench, records) == 32, "capture header size");
_Static_assert(sizeof(struct swr_bench_record) == 16, "capture record size");
_Static_assert(offsetof(struct swr_bench_record, tx_flag) == 14, "capture TX field offset");
_Static_assert(offsetof(struct swr_bench_record, adc_cursor) == 15, "capture cursor offset");
_Static_assert(SWR_BENCH_WIRE_SIZE == 1632, "capture export size");
_Static_assert(sizeof(struct swr_bench) <= 2048, "capture RAM reservation");

void swr_bench_init(volatile struct swr_bench *bench);
/* The caller samples at 25 Hz from the stock 100 Hz timebase. */
void swr_bench_sample(volatile struct swr_bench *bench, uint32_t tick, uint8_t tx_flag,
                      uint8_t adc_cursor, const uint16_t adc[SWR_BENCH_ADC_COUNT]);

#endif
