#include "h9_main_core_post_samples.h"

#include <stddef.h>

static uint32_t call(const H9MainCorePostSamples *post, uint32_t target,
                     uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3)
{
    return post->invoke(post->context, target, r0, r1, r2, r3);
}

bool h9_main_core_post_samples_run(const H9MainCorePostSamples *post)
{
    uint32_t initialized;
    if (post == NULL || post->module_11bc0 == NULL ||
        post->list_12f38 == NULL || post->state_12188 == NULL ||
        post->invoke == NULL) return false;

    (void)call(post, 0x01E2475Au, 23u, 0u, 0x01E8A908u, 0u);
    (void)call(post, 0x02003904u, 0u, 0x01E8A826u, 2u, 0u);
    (void)call(post, 0x01E60776u, 0u, 0u, 0u, 0u);
    *post->module_11bc0 = 0x01EB1361u;
    (void)call(post, 0x01E24F92u, 0u, 0u, 0u, 0u);

    (void)call(post, 0x021127B4u, 0x00012F40u, 0u, 0xA4u, 0u);
    post->list_12f38[0] = 0x00012F38u;
    post->list_12f38[1] = 0x00012F38u;
    (void)call(post, 0x02002CAEu, 0x00012F38u, 0u, 0u, 0u);
    (void)call(post, 0x02002948u, 0x00012F90u, 0u, 0u, 0u);
    initialized = call(post, 0x01E2D022u, 0x01E4A970u, 0u,
                       0x01EB17DFu, 0u);
    if (initialized != 0u)
        (void)call(post, 0x01E4A674u, 0u, 0u, 0u, 0u);
    (void)call(post, 0x02002B66u, 0x00012F90u, 0u, 0u, 0u);
    (void)call(post, 0x01E5176Eu, *post->state_12188, 0u, 0u, 0u);
    return true;
}
