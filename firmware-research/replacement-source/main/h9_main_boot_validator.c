#include "h9_main_boot_validator.h"

#include <stddef.h>
#include <stdint.h>

uint32_t h9_main_boot_validate(const volatile uint8_t *data, size_t length,
                               void *context)
{
    H9MainBootValidator *validator = (H9MainBootValidator *)context;
    uint32_t result;
    size_t index;
    if (data == NULL || length != H9_MAIN_BOOT_RECORD_CHECK_BYTES ||
        validator == NULL || validator->stream_data == NULL ||
        validator->stream_control == NULL || validator->result_word == NULL ||
        validator->begin == NULL || validator->synchronize == NULL ||
        validator->finish == NULL) return 0u;

    validator->begin(validator->context, H9_MAIN_BOOT_DESCRIPTOR, 0u);
    *validator->stream_control = 0u;
    for (index = 0; index < length; ++index)
        *validator->stream_data = data[index];
    validator->synchronize(validator->context);
    result = *validator->result_word;
    validator->finish(validator->context, H9_MAIN_BOOT_DESCRIPTOR);
    return result & 0xFFFFu;
}

#if defined(__PI32V2__)
static void target_begin(void *context, uint32_t descriptor, uint32_t mode)
{
    typedef void (*RomBegin)(uint32_t, uint32_t);
    (void)context;
    ((RomBegin)(uintptr_t)0x02002BC4u)(descriptor, mode);
}

static void target_synchronize(void *context)
{
    (void)context;
    __asm__ volatile("csync" ::: "memory");
}

static void target_finish(void *context, uint32_t descriptor)
{
    typedef void (*RomFinish)(uint32_t);
    (void)context;
    ((RomFinish)(uintptr_t)0x02002C3Cu)(descriptor);
}
#endif

bool h9_main_boot_validator_bind_target(H9MainBootValidator *validator)
{
    if (validator == NULL) return false;
#if defined(__PI32V2__)
    validator->stream_data = (volatile uint32_t *)(uintptr_t)0x001E3500u;
    validator->stream_control = (volatile uint32_t *)(uintptr_t)0x001E3504u;
    validator->result_word = (volatile uint32_t *)(uintptr_t)0x00012854u;
    validator->begin = target_begin;
    validator->synchronize = target_synchronize;
    validator->finish = target_finish;
    validator->context = NULL;
    return true;
#else
    return false;
#endif
}
