#include "capture.h"

void swr_bench_init(volatile struct swr_bench *bench)
{
    volatile uint8_t *bytes = (volatile uint8_t *)bench;
    bench->magic = 0;
    for (unsigned i = sizeof bench->magic; i < sizeof *bench; ++i)
        bytes[i] = 0;
    bench->version = SWR_BENCH_VERSION;
    bench->record_size = sizeof(struct swr_bench_record);
    bench->magic = SWR_BENCH_MAGIC;
}

void swr_bench_sample(volatile struct swr_bench *bench, uint32_t tick, uint8_t tx_flag,
                      uint8_t adc_cursor, const uint16_t adc[SWR_BENCH_ADC_COUNT])
{
    unsigned status = bench->status;
    if (status >= SWR_BENCH_COMPLETE)
        return;

    unsigned tx = tx_flag == 1;
    unsigned pretrigger = status == SWR_BENCH_ARMED && !tx;
    unsigned index = pretrigger ? bench->pre_head : SWR_BENCH_PRE + bench->count;
    volatile struct swr_bench_record *record = &bench->records[index];
    record->tick = tick;
    for (unsigned i = 0; i < SWR_BENCH_ADC_COUNT; ++i)
        record->adc[i] = adc[i];
    record->tx_flag = tx_flag;
    record->adc_cursor = adc_cursor;

    if (pretrigger) {
        bench->pre_head = (index + 1) & (SWR_BENCH_PRE - 1);
        if (bench->pre_count < SWR_BENCH_PRE)
            ++bench->pre_count;
        return;
    }

    bench->count = index - SWR_BENCH_PRE + 1;
    bench->post_count = tx ? 0 : bench->post_count + 1;
    if (bench->post_count == SWR_BENCH_POST)
        bench->status = SWR_BENCH_COMPLETE;
    else if (index == SWR_BENCH_RECORDS - 1)
        bench->status = SWR_BENCH_LIMIT;
    else if (status == SWR_BENCH_ARMED)
        bench->status = SWR_BENCH_CAPTURING;
}
