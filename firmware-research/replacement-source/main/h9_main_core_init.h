#ifndef H9_MAIN_CORE_INIT_H
#define H9_MAIN_CORE_INIT_H

#include <stdint.h>

/* Stock app_core at app+0x066648 calls these ten entries in four groups,
 * with a direct app+0x060AF8 call between groups two and three. These are
 * stock addresses for dispatch identification, not live replacement code. */
enum { H9_MAIN_CORE_INIT_COUNT = 10, H9_MAIN_EVENT_RECORD_COUNT = 3 };
extern const uint32_t h9_main_core_init_stock_entries[H9_MAIN_CORE_INIT_COUNT];

typedef struct {
    void *context;
    void (*invoke_initializer)(void *context, uint32_t stock_entry);
    void (*between_groups)(void *context); /* stock app+0x060AF8 */
} H9MainCoreInitHooks;

int h9_main_core_run_initializers(const H9MainCoreInitHooks *hooks);

/* Stock app+0x02C514 walks three 28-byte records and invokes each first
 * word when nonzero. The remaining words are preserved for later event-loop
 * reconstruction; their meanings have not been established. */
typedef struct {
    uint32_t word[7];
} H9MainEventRecord;

extern const H9MainEventRecord
    h9_main_event_records[H9_MAIN_EVENT_RECORD_COUNT];

typedef struct {
    void *context;
    void (*invoke_callback)(void *context, uint32_t stock_entry);
} H9MainEventRecordHooks;

int h9_main_core_run_event_records(const H9MainEventRecordHooks *hooks);

#endif
