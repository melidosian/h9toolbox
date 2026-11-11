/* The satellite table, read from the hidden module ".SAT DATA" of the bundle (built by satpack.py,
 * or in the browser by the App Manager's satellite settings: dist/apps/satdata.mjs).
 *
 * Layout (little endian), the module's body:
 *   32-byte header: u32 "SATD", u16 version (1), u16 count (1-16), u32 shared epoch (Unix), u32 GMST
 *   at that epoch (binary angle), u32 newest element set (Unix), char date[12] ("2026-11-10").
 *   then `count` entries of 64 bytes: struct sat (52) and struct satfreq (12).
 * Modules read what they need through the loader's NOR read (api->read) instead of carrying the
 * table: the table can change without rebuilding any code, and the code modules got smaller. */
#ifndef SATDATA_H
#define SATDATA_H

#include <stdint.h>

#include "h9mod.h"
#include "orbit.h"

#define SATDATA_MAGIC 0x44544153u   /* "SATD" */
enum { SATDATA_HEAD = 32, SATDATA_ENTRY = 64, SATDATA_MAX = 16 };

struct satdata_head {
    uint32_t magic;
    uint16_t version, count;
    uint32_t gmst_unix, gmst_angle, newest;
    char date[12];
};

struct satentry {
    struct sat sat;
    struct satfreq freq;
};

struct satdata {
    const struct h9_api *api;
    uint32_t base;                 /* chip address of the data module's body */
    struct satdata_head head;
};

/* Find ".SAT DATA" in the bundle and read its header; 0 if it is missing or not valid. */
static inline int satdata_open(const struct h9_api *api, struct satdata *d)
{
    static const char NAME[] = ".SAT DATA";
    uint8_t header[H9_BUNDLE_HEADER];
    d->api = api;
    d->head.count = 0;
    api->read(header, api->bundle, sizeof header);
    unsigned count = header[10] | (unsigned)header[11] << 8;
    for (unsigned i = 1; i < count && i < 64; i++) {
        struct h9_entry e;
        unsigned k = 0;
        api->read(&e, api->bundle + H9_BUNDLE_HEADER + H9_BUNDLE_ENTRY * i, sizeof e);
        while (k < H9_NAME_BYTES && e.name[k] == (k < sizeof NAME ? NAME[k] : 0)) k++;
        if (k < H9_NAME_BYTES) continue;
        d->base = api->bundle + e.offset;
        api->read(&d->head, d->base, sizeof d->head);
        if (d->head.magic != SATDATA_MAGIC || d->head.count > SATDATA_MAX) { d->head.count = 0; return 0; }
        return d->head.count != 0;
    }
    return 0;
}

static inline void satdata_get(const struct satdata *d, unsigned i, struct satentry *e)
{
    d->api->read(e, d->base + SATDATA_HEAD + SATDATA_ENTRY * (i < d->head.count ? i : 0), sizeof *e);
}

static inline void satdata_gmst(const struct satdata *d, struct gmst_ref *g)
{
    g->unix = d->head.gmst_unix;
    g->angle = d->head.gmst_angle;
}

#endif
