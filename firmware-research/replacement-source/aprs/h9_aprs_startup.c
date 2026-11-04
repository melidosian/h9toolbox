#include <stddef.h>
#include <stdint.h>

#include "h9_aprs_clock.h"

/* Stock image: 48 vectors are copied to RAM 0x20000000 and VTOR is set to
 * that address. Our own data is uncompressed, so its initialization differs
 * from the stock image's two-record decompression/zero table. */
#define H9_APRS_VECTOR_COUNT 48u
#define H9_APRS_VECTOR_RAM ((volatile uint32_t *)0x20000000u)
#define H9_APRS_VTOR (*(volatile uint32_t *)0xE000ED08u)

extern const uint32_t h9_aprs_vectors[H9_APRS_VECTOR_COUNT];
extern uint8_t __data_load_start;
extern uint8_t __data_start;
extern uint8_t __data_end;
extern uint8_t __bss_start;
extern uint8_t __bss_end;

/* A board integration unit must implement this; the link check supplies an
 * inert loop and therefore does not produce functional firmware. */
void h9_aprs_board_main(void);

__attribute__((noreturn)) void h9_aprs_boot(void)
{
    const uint8_t *source = &__data_load_start;
    uint8_t *destination;
    size_t index;
    h9_aprs_clock_init();
    for (destination = &__data_start; destination < &__data_end;
         ++destination, ++source)
        *destination = *source;
    for (destination = &__bss_start; destination < &__bss_end;
         ++destination)
        *destination = 0;
    for (index = 0; index < H9_APRS_VECTOR_COUNT; ++index)
        H9_APRS_VECTOR_RAM[index] = h9_aprs_vectors[index];
    H9_APRS_VTOR = 0x20000000u;
    h9_aprs_board_main();
    for (;;) { }
}
