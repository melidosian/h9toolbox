/* Host test: fix_store.c reads records written the way the loader stub's
 * background recorder (src/stub.s rec_save) writes them. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void host_erase(uint32_t sector);
static void host_program(const void *src, uint32_t address, uint32_t length);
#define FIX_WITH_SAVE
#define NOR_ERASE host_erase
#define NOR_PROGRAM host_program
#include "fix_store.c"

static uint8_t chip[4096];
static int erases;
static void host_erase(uint32_t sector) { (void)sector; memset(chip, 0xFF, sizeof chip); erases++; }
static void host_program(const void *src, uint32_t address, uint32_t length) { for (uint32_t i = 0; i < length; i++) chip[address - FIX_STORE_SECTOR + i] &= ((const uint8_t *)src)[i]; }
static void chip_read(void *dst, uint32_t address, uint32_t length)
{
    memcpy(dst, chip + (address - FIX_STORE_SECTOR), length);
}

/* rec_save in src/stub.s, step by step: x = lat ^ lon ^ 0x5A5A (day 0);
 * x ^= x >> 16; word 3 = x << 16 (day = 0, check = low half of x). */
static void stub_record(unsigned index, int32_t lat, int32_t lon)
{
    uint32_t w[4], x = (uint32_t)lat ^ (uint32_t)lon ^ 0x5A5Au;
    x ^= x >> 16;
    w[0] = 0x31584647u; w[1] = (uint32_t)lat; w[2] = (uint32_t)lon; w[3] = x << 16;
    memcpy(chip + index * 16, w, 16);
}

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
    int32_t lat, lon;
    memset(chip, 0xFF, sizeof chip);
    CHECK(!fix_load_micro(chip_read, &lat, &lon));
    stub_record(0, 41878000, -87630000);
    CHECK(fix_load_micro(chip_read, &lat, &lon));
    CHECK(lat == 41878000 && lon == -87630000);
    CHECK(fix_bam(lat) > 499623000 && fix_bam(lat) < 499625000);   /* 41.878 * 2^32 / 360 = 499,624,001 */
    stub_record(1, -33869000, 151209000);
    CHECK(fix_load_micro(chip_read, &lat, &lon));
    CHECK(lat == -33869000 && lon == 151209000);
    memcpy(chip + 2 * 16, "GFX1", 4);            /* torn record: magic only, rest erased */
    CHECK(fix_load_micro(chip_read, &lat, &lon) && lat == -33869000);   /* still record 1 */
    /* the module-side save appends, the newest wins, a full sector is erased once */
    memset(chip, 0xFF, sizeof chip);
    fix_save(chip_read, 41878000, -87630000);
    CHECK(fix_load_micro(chip_read, &lat, &lon) && lat == 41878000 && lon == -87630000);
    fix_save(chip_read, -33869000, 151209000);
    CHECK(fix_load_micro(chip_read, &lat, &lon) && lat == -33869000 && erases == 0);
    for (int n = 0; n < 300; n++) fix_save(chip_read, 1000000 + n, 2000000);
    CHECK(fix_load_micro(chip_read, &lat, &lon) && lat == 1000299 && erases == 1);
    CHECK(fix_micro(fix_bam(41878000)) > 41877990 && fix_micro(fix_bam(41878000)) < 41878010);
    printf(failures ? "%d failure(s)\n" : "fix_store ok\n", failures);
    return failures != 0;
}
