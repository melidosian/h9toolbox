// SPDX-License-Identifier: GPL-2.0-or-later
// TD-H9 port of DMC100-Doom: the NOR driver and the FAT12 lookups of the data
// layer, mapped onto the TD-H9 16 MiB resource flash.
//
// The engine reads its pack through the data layer (port/data), which finds
// DOOM.DAT as a list of 4 KiB cluster runs on a FAT12 drive. Here there is no
// FAT: the pack was written raw by prototypes/data-upload, with a directory
// sector at 0xA1F000 (magic H9DATA01, CRC-16, COMPLETE marker in its last
// block). fat_mount/fat_open present that pack as DOOM.DAT, one run starting
// at its chip address, so the data layer's offsets map straight to chip
// addresses. nor_read() reads them with the stock SPI2 reader (sync;
// the async and column calls complete at once, like DMC100's NOR_POLLED
// build).
//
// Function contracts follow DMC100-Doom's platform/nor.h and port/data/fat12.h.

#include <string.h>

#include "extents.h"
#include "fat12.h"
#include "nor.h"

#include "td.h"

volatile struct nor_stats nor_stats;

// The resource flash as stock my_app sets it up (app+0x08849C-0x0884BC):
// chip select GPIO 23 (bank 1 bit 7) as a high output, then SPI2. Stock then
// reads the JEDEC ID through a my_app-only helper and marks the chip ready
// for its partition reader; that helper is overwritten in this firmware, so
// reads go to the lower-level stock SPI2 read instead.
void td_nor_setup(void)
{
#ifndef TD_HOST
    volatile uint32_t *bank1 = (volatile uint32_t *)0x001e5040;
    bank1[3] |= 0x80;   // +0x0C input enable
    bank1[2] &= ~0x80u; // +0x08 direction: output
    bank1[0] |= 0x80;   // +0x00 high: deselected
#endif
    h9_spi2_setup();
}

uint32_t td_nor_bytes;  // build 30: bytes read, for the frame readout

// Stock's read (app+0x4FE22) uses the SPI DMA only for reads of 16 bytes or
// more into a 4-byte-aligned buffer; anything else goes a byte at a time
// through a register write and a wait call. Build 31: short or unaligned
// reads go through an aligned bounce buffer, so every read takes the DMA
// path (build 30: a far view read 75 KB a frame).
static uint8_t bounce[512] __attribute__((aligned(4)));

static void read_dma(uint32_t phys, void *dst, uint32_t n)
{
    uint8_t *out = dst;
    if (!((uintptr_t)out & 3) && n >= 16) {
        h9_nor_read_raw(out, phys, n);
        return;
    }
    while (n) {
        uint32_t take = n < sizeof bounce ? n : sizeof bounce;
        uint32_t get = take < 16 ? 16 : take;
        if (phys + get > 0x1000000u) {
            h9_nor_read_raw(out, phys, take);  // the chip's last bytes: stock's byte path
        } else {
            h9_nor_read_raw(bounce, phys, get);
            memcpy(out, bounce, take);
        }
        out += take;
        phys += take;
        n -= take;
    }
}

void nor_read(uint32_t phys, void *dst, uint32_t n)
{
    if (phys >= 0x1000000u || n > 0x1000000u - phys)
        td_fatal("NOR READ", "outside the chip");
    if (n)
        read_dma(phys, dst, n);
    td_nor_bytes += n;
    nor_stats.sync++;
    td_nor_reads++;
    td_trace_counters(0);
}

int nor_read_async(uint32_t phys, void *dst, uint32_t n, int prio)
{
    (void)prio;
    nor_read(phys, dst, n);
    nor_stats.async_chunks++;
    return 1;
}

bool nor_done(int ticket)
{
    (void)ticket;
    return true;
}

void nor_wait(int ticket)
{
    (void)ticket;
}

void nor_wait_all(void)
{
}

int nor_col_start(uint32_t phys, void *dst, uint32_t n)
{
    if (!n || n > 256)
        return -1;
    nor_read(phys, dst, n);
    return 0;
}

void nor_col_wait(void)
{
}

int nor_init(void)
{
    return 0;
}

// ---- the upload directory (prototypes/data-upload/upload.mjs)

static uint16_t crc16(const uint8_t *p, unsigned n)
{
    uint16_t crc = 0;
    while (n--) {
        crc ^= (uint16_t)(*p++ << 8);
        for (int b = 0; b < 8; b++)
            crc = crc & 0x8000 ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

static uint32_t rd32(const uint8_t *p)
{
    return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

// The data's chip address and length from the COMPLETE upload directory at
// `directory` (one segment). 0 on success, a negative code otherwise.
int td_dir_find(uint32_t directory, uint32_t *address, uint32_t *length)
{
    uint8_t d[128];
    uint8_t done[16];
    nor_read(directory, d, sizeof d);
    nor_read(directory + 0x1000 - 32, done, sizeof done);
    if (memcmp(d, "H9DATA01", 8))
        return -1;  // no upload
    if (memcmp(done, "H9DATA-COMPLETE", 15))
        return -2;  // upload not finished
    uint32_t count = rd32(d + 80);
    if (count != 1)
        return -3;  // first pass: one segment only
    unsigned end = 84 + 8 * count;
    if ((d[end] | d[end + 1] << 8) != crc16(d, end))
        return -4;  // directory damaged
    *address = rd32(d + 84);
    *length = rd32(d + 88);
    if (*address < TD_RESOURCE_ORIGIN || *address + *length > TD_RESOURCE_END || (*address & 0xFFF))
        return -5;
    return 0;
}

int td_pack_find(uint32_t *address, uint32_t *length)
{
    return td_dir_find(TD_DIRECTORY, address, length);
}

// ---- FAT12 lookups of the data layer

static uint32_t pack_address, pack_length;

int fat_mount(fat_vol_t *v, uint32_t base)
{
    if (base != DMC_VOL_MAIN)
        return FAT_E_BPB;  // no firmware drive: no DOOMX.DAT or DOOMCODE.DMC
    if (td_pack_find(&pack_address, &pack_length))
        return FAT_E_BPB;
    memset(v, 0, sizeof *v);
    v->data = pack_address;
    v->cshift = 12;
    v->nclus = (uint16_t)((pack_length + 0xFFF) >> 12);
    return FAT_OK;
}

int fat_open(const fat_vol_t *v, const char *name83, dmc_file_t *f)
{
    if (memcmp(name83, "DOOM    DAT", 11))
        return FAT_E_NOFILE;
    f->data = v->data;
    f->cshift = 12;
    f->next = 0;
    f->size = 0;
    for (uint32_t k = 0; k < v->nclus; k++)
        if (ext_add(f, (uint16_t)k, (uint16_t)(k + 2)))
            return FAT_E_FRAG;
    f->size = pack_length;
    return FAT_OK;
}

int fat_dir(const fat_vol_t *v, const char *name83, uint32_t *addr)
{
    (void)v;
    (void)name83;
    (void)addr;
    return FAT_E_NOFILE;
}
