/* Host test helper: a fake bundle holding the ".SAT DATA" module (a file made by satpack.py) behind
 * the loader's read function, so satdata.h (and everything built on it) runs unchanged on the host.
 * Chip addresses from HOST_BUNDLE_BASE up are the bundle; the others go to `other_read` (a test's
 * own fake chip, or NULL). */
#ifndef HOST_SATDATA_H
#define HOST_SATDATA_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "satdata.h"

#define HOST_BUNDLE_BASE 0xCC1000u

static uint8_t *host_bundle;
static void (*host_other_read)(void *dst, uint32_t address, uint32_t length);

static void host_bundle_read(void *dst, uint32_t address, uint32_t length)
{
    if (address >= HOST_BUNDLE_BASE) memcpy(dst, host_bundle + (address - HOST_BUNDLE_BASE), length);
    else if (host_other_read) host_other_read(dst, address, length);
    else memset(dst, 0xFF, length);
}

static struct h9_api host_bundle_api = { 0x50413948u, 1, HOST_BUNDLE_BASE, 0x29F00u, 0x2000u, host_bundle_read };

/* Build the bundle in memory around `path` (the data module body); returns the loader API. */
static const struct h9_api *host_satdata_api(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(2); }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    host_bundle = calloc(1, 16 + 64 + (size_t)size);
    memcpy(host_bundle, "H9MODS01", 8);
    host_bundle[8] = 1;                       /* version */
    host_bundle[10] = 2;                      /* a stand-in menu and the data module */
    unsigned total = 16 + 64 + (unsigned)size;
    memcpy(host_bundle + 12, &total, 4);
    memcpy(host_bundle + 16, "MENU", 4);
    uint8_t *e = host_bundle + 16 + 32;
    memcpy(e, ".SAT DATA", 9);
    unsigned offset = 16 + 64, sz = (unsigned)size;
    memcpy(e + 16, &offset, 4);
    memcpy(e + 20, &sz, 4);
    if (fread(host_bundle + offset, 1, (size_t)size, f) != (size_t)size) exit(2);
    fclose(f);
    return &host_bundle_api;
}

static void host_get_sat(void *ctx, unsigned i, struct sat *out)
{
    struct satentry entry;
    satdata_get((const struct satdata *)ctx, i, &entry);
    *out = entry.sat;
}

#endif
