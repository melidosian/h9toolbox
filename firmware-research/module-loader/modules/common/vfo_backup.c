/* VFO backup records on the resource chip. See vfo_backup.h. */
#include "vfo_backup.h"

#ifndef NOR_ERASE   /* the stock sector erase / page program, as fix_store.c (hardware-proven uploads) */
#define NOR_ERASE ((void (*)(uint32_t sector))0x01E50000)
#define NOR_PROGRAM ((void (*)(const void *src, uint32_t chip_address, uint32_t length))0x01E5006E)
#endif

enum { REC = 32, COUNT = 4096 / REC };
#define VFB_MAGIC 0x32424656u  /* "VFB2" (the 16-byte "VFB1" records without tones are ignored) */

struct vfb_rec {
    uint32_t magic, a, b;
    uint16_t tone_a, tone_b, state, check;
    uint32_t pad[3];
};

static uint16_t check_of(const struct vfb_rec *r)
{
    uint32_t x = r->a ^ r->b ^ r->state ^ r->tone_a ^ ((uint32_t)r->tone_b << 7) ^ 0xA55Au;
    return (uint16_t)(x ^ (x >> 16));
}

/* First erased record (COUNT if full); *last = newest valid (magic 0 if none). */
static unsigned scan(fix_read_fn read, struct vfb_rec *last)
{
    struct vfb_rec r;
    unsigned i;
    last->magic = 0;
    for (i = 0; i < COUNT; i++) {
        read(&r, VFO_BACKUP_SECTOR + i * REC, REC);
        if (r.magic == 0xFFFFFFFFu) break;
        if (r.magic == VFB_MAGIC && r.check == check_of(&r)) *last = r;
    }
    return i;
}

unsigned vfo_backup_load(fix_read_fn read, uint32_t *a, uint32_t *b, uint16_t *tone_a, uint16_t *tone_b)
{
    struct vfb_rec r;
    scan(read, &r);
    if (!r.magic) return 0;
    *a = r.a; *b = r.b; *tone_a = r.tone_a; *tone_b = r.tone_b;
    return r.state;
}

void vfo_backup_save(fix_read_fn read, uint32_t a, uint32_t b, uint16_t tone_a, uint16_t tone_b, unsigned state)
{
    struct vfb_rec last, r;
    unsigned i = scan(read, &last);
    r.magic = VFB_MAGIC; r.a = a; r.b = b; r.tone_a = tone_a; r.tone_b = tone_b; r.state = (uint16_t)state;
    r.pad[0] = r.pad[1] = r.pad[2] = 0xFFFFFFFFu;
    r.check = check_of(&r);
    if (i == COUNT) { NOR_ERASE(VFO_BACKUP_SECTOR >> 12); i = 0; }
    NOR_PROGRAM(&r, VFO_BACKUP_SECTOR + i * REC, REC);
}
