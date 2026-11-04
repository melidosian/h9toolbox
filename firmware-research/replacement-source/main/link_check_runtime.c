#include <stddef.h>

/* Diagnostic link shims. This entry initializes C memory but does not
 * initialize the TD-H9 board or implement its production runtime. */
extern unsigned char __data_load_start[];
extern unsigned char __data_start[];
extern unsigned char __data_end[];
extern unsigned char __bss_start[];
extern unsigned char __bss_end[];
void *memcpy(void *destination, const void *source, size_t length)
{
    unsigned char *to = (unsigned char *)destination;
    const unsigned char *from = (const unsigned char *)source;
    size_t index;
    for (index = 0; index < length; ++index) to[index] = from[index];
    return destination;
}

void *memset(void *destination, int value, size_t length)
{
    unsigned char *to = (unsigned char *)destination;
    size_t index;
    for (index = 0; index < length; ++index) to[index] = (unsigned char)value;
    return destination;
}

__attribute__((noreturn)) void h9_main_board_entry(void)
{
    for (;;) { }
}

__attribute__((noreturn)) void h9_main_runtime_init(void)
{
    unsigned char *to;
    unsigned char *from = __data_load_start;
    for (to = __data_start; to < __data_end; ++to) *to = *from++;
    for (to = __bss_start; to < __bss_end; ++to) *to = 0;
    h9_main_board_entry();
}
