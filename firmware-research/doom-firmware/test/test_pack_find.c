// Host test of td_pack_find (port/td_nor.c) against directory sectors made by
// the uploader's own code (make_directory.mjs).
//   cc -std=gnu11 -I<dmc>/port/data -I<dmc>/platform -I<dmc>/engine -I../port test_pack_find.c ../port/td_nor.c <dmc>/port/data/extents.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "td.h"

static unsigned char chip[0x300000];  // resource partition
void td_fatal(const char *a, const char *b) { printf("fatal %s %s\n", a, b); exit(1); }
void h9_nor_read_raw(void *buf, uint32_t addr, uint32_t n)
{
    if (addr < 0xA00000 || addr + n > 0xA00000 + sizeof chip) { printf("bad read %x %u\n", addr, n); exit(1); }
    memcpy(buf, chip + (addr - 0xA00000), n);
}
void h9_spi2_setup(void) {}

static int load(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    size_t n = fread(chip + 0x1F000, 1, 0x1000, f);
    fclose(f);
    return n == 0x1000 ? 0 : -1;
}

int main(int argc, char **argv)
{
    uint32_t a, l;
    int fails = 0;
    memset(chip, 0xff, sizeof chip);
    if (td_pack_find(&a, &l) != -1) { puts("FAIL erased directory"); fails++; }
    if (load(argv[1])) { puts("no fixture"); return 1; }
    int r = td_pack_find(&a, &l);
    if (r || a != 0xA20000 || l != (uint32_t)atoi(argv[2])) { printf("FAIL complete: %d %x %u\n", r, a, l); fails++; }
    else printf("ok: pack at 0x%X, %u bytes\n", a, l);
    chip[0x1F000 + 0xFE0] = 0xff;  // not complete
    if (td_pack_find(&a, &l) != -2) { puts("FAIL in-progress accepted"); fails++; }
    chip[0x1F000 + 0xFE0] = 'H';
    chip[0x1F000 + 20] ^= 1;  // inside the CRC-covered header (SHA bytes)
    if (td_pack_find(&a, &l) != -4) { puts("FAIL corrupt accepted"); fails++; }
    puts(fails ? "FAILED" : "PASS");
    return fails != 0;
}
