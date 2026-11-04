#include <stddef.h>

/* Diagnostic link shims. Neither this entry nor these routines initialize
 * the TD-H9 board or implement its production runtime. */
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

__attribute__((noreturn, section(".text.h9_link_check_entry")))
void h9_link_check_entry(void)
{
    for (;;) { }
}
