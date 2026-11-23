#include "capture.h"

#include <assert.h>
#include <string.h>

static const uint16_t adc[SWR_BENCH_ADC_COUNT] = {10, 30, 40, SWR_BENCH_MISSING, 130};

static void test_manual_tx(void)
{
    struct swr_bench bench;
    memset(&bench, 0xa5, sizeof bench);
    swr_bench_init(&bench);
    assert(bench.magic == SWR_BENCH_MAGIC);
    assert(bench.version == SWR_BENCH_VERSION);
    assert(bench.record_size == 16);
    for (unsigned i = 0; i < sizeof bench.reserved; ++i)
        assert(bench.reserved[i] == 0);
    assert(bench.records[99].adc_cursor == 0);

    for (unsigned i = 0; i < 20; ++i)
        swr_bench_sample(&bench, 4 * i, 0, i % 5, adc);
    assert(bench.status == SWR_BENCH_ARMED && bench.count == 0);
    assert(bench.pre_count == 16 && bench.pre_head == 4);
    for (unsigned i = 0; i < 16; ++i)
        assert(bench.records[(bench.pre_head + i) % 16].tick == 4 * (4 + i));

    swr_bench_sample(&bench, 80, 1, 3, adc);
    assert(bench.status == SWR_BENCH_CAPTURING && bench.count == 1);
    struct swr_bench_record *first = &bench.records[16];
    assert(first->tick == 80 && first->tx_flag == 1 && first->adc_cursor == 3);
    assert(memcmp(first->adc, adc, sizeof adc) == 0);

    for (unsigned i = 0; i < 12; ++i)
        swr_bench_sample(&bench, 84 + 4 * i, 0, i % 5, adc);
    assert(bench.status == SWR_BENCH_COMPLETE && bench.count == 13);
    const struct swr_bench saved = bench;
    swr_bench_sample(&bench, 132, 1, 0, adc);
    assert(memcmp(&bench, &saved, SWR_BENCH_WIRE_SIZE) == 0);
    assert(bench.post_count == saved.post_count);
}

static void test_short_pretrigger_and_repeat(void)
{
    struct swr_bench bench;
    swr_bench_init(&bench);
    swr_bench_sample(&bench, UINT32_MAX - 3, 2, 0, adc);
    assert(bench.pre_head == 1 && bench.pre_count == 1);
    assert(bench.records[0].tx_flag == 2);
    swr_bench_sample(&bench, 0, 1, 0, adc);
    swr_bench_sample(&bench, 4, 0, 1, adc);
    swr_bench_sample(&bench, 8, 1, 2, adc);
    for (unsigned i = 0; i < 11; ++i)
        swr_bench_sample(&bench, 12 + 4 * i, 0, 0, adc);
    assert(bench.status == SWR_BENCH_CAPTURING && bench.count == 14);
    swr_bench_sample(&bench, 56, 0, 0, adc);
    assert(bench.status == SWR_BENCH_COMPLETE && bench.count == 15);
}

static void test_held_tx_limit(void)
{
    struct swr_bench bench;
    swr_bench_init(&bench);
    for (unsigned i = 0; i < 100; ++i)
        swr_bench_sample(&bench, 4 * i, 1, 0, adc);
    assert(bench.status == SWR_BENCH_LIMIT && bench.count == 84);
    assert(bench.pre_count == 0 && bench.pre_head == 0);
    assert(bench.records[99].tick == 4 * 83);
}

int main(void)
{
    test_manual_tx();
    test_short_pretrigger_and_repeat();
    test_held_tx_limit();
}
