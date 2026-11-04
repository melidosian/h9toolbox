#include "h9_main_core_init.h"

#include <stddef.h>

const uint32_t h9_main_core_init_stock_entries[H9_MAIN_CORE_INIT_COUNT] = {
    0x01E5E53Eu, 0x01E48B46u, 0x01E463F8u, 0x01E5F890u,
    0x01E2D5D8u, 0x01E253B8u, 0x01E907E4u, 0x01E9063Eu,
    0x01E90432u, 0x01E2C614u,
};

static void run_group(const H9MainCoreInitHooks *hooks,
                      unsigned first, unsigned end)
{
    unsigned index;
    for (index = first; index < end; ++index)
        hooks->invoke_initializer(
            hooks->context, h9_main_core_init_stock_entries[index]);
}

int h9_main_core_run_initializers(const H9MainCoreInitHooks *hooks)
{
    if (hooks == NULL || hooks->invoke_initializer == NULL ||
        hooks->between_groups == NULL) return -1;
    run_group(hooks, 3u, 6u);
    run_group(hooks, 9u, 10u);
    hooks->between_groups(hooks->context);
    run_group(hooks, 0u, 3u);
    /* Stock's fourth range has equal start and end addresses. */
    run_group(hooks, 6u, 9u);
    return 0;
}

const H9MainEventRecord
    h9_main_event_records[H9_MAIN_EVENT_RECORD_COUNT] = {
    { { 0x01E2C962u, 0x01E2C9F4u, 0x01E2CA0Cu, 0x01E2CA5Cu,
        0u, 0u, 0u } },
    { { 0x01E8CAD4u, 0x01E8CDA6u, 0x01E6285Eu, 0x01E62B56u,
        0x01E8CDB4u, 0u, 0u } },
    { { 0x01E2C64Cu, 0x01E2C718u, 0x01E2C848u, 0u,
        0u, 0x01E2C83Au, 0x01E2C88Au } },
};

int h9_main_core_run_event_records(const H9MainEventRecordHooks *hooks)
{
    unsigned index;
    if (hooks == NULL || hooks->invoke_callback == NULL) return -1;
    for (index = 0u; index < H9_MAIN_EVENT_RECORD_COUNT; ++index) {
        uint32_t entry = h9_main_event_records[index].word[0];
        if (entry != 0u)
            hooks->invoke_callback(hooks->context, entry);
    }
    return 0;
}
