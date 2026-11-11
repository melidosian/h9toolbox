/* Last GPS fix on the resource chip. See fix_store.h. */
#include "fix_store.h"

enum { REC = 16, COUNT = 4096 / REC };
#define FIX_MAGIC 0x31584647u  /* "GFX1" */

struct fix_rec {
    uint32_t magic;
    int32_t lat, lon;
    uint16_t day, check;
};

static uint16_t check_of(const struct fix_rec *r)
{
    uint32_t x = (uint32_t)r->lat ^ (uint32_t)r->lon ^ r->day ^ 0x5A5Au;
    return (uint16_t)(x ^ (x >> 16));
}

/* The first erased record (COUNT if the sector is full); *last = the newest
 * valid record (magic 0 if none). A torn record fails its check and is
 * skipped. */
static unsigned scan(fix_read_fn read, struct fix_rec *last)
{
    struct fix_rec r;
    unsigned i;
    last->magic = 0;
    for (i = 0; i < COUNT; i++) {
        read(&r, FIX_STORE_SECTOR + i * REC, REC);
        if (r.magic == 0xFFFFFFFFu) break;
        if (r.magic == FIX_MAGIC && r.check == check_of(&r)) *last = r;
    }
    return i;
}

/* The newest saved fix in signed microdegrees. */
int fix_load_micro(fix_read_fn read, int32_t *lat, int32_t *lon)
{
    struct fix_rec r;
    scan(read, &r);
    if (!r.magic) return 0;
    *lat = r.lat; *lon = r.lon;
    return 1;
}
