#include <stddef.h>

/* Temporary freestanding shims for a whole-archive link. Actual startup and
   board initialization remain to be implemented before any firmware build. */
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

__attribute__((noreturn)) void Reset_Handler(void)
{
    for (;;) { }
}

__attribute__((noreturn)) void Default_Handler(void)
{
    for (;;) { }
}
