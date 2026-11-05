#include "h9_main_core_post_samples.h"

#include <stddef.h>

static uint32_t call(const H9MainCorePostSamples *post, uint32_t target,
                     uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3)
{
    return post->invoke(post->context, target, r0, r1, r2, r3);
}

bool h9_main_core_post_samples_run(const H9MainCorePostSamples *post)
{
    int task_status;
    unsigned index;
    if (post == NULL || post->module_11bc0 == NULL ||
        post->list_12f38 == NULL || post->storage_12f40 == NULL ||
        post->state_12188 == NULL || post->task_hooks == NULL ||
        post->task_hooks->create == NULL || post->dev_mg_entry == 0u ||
        post->invoke == NULL) return false;

    (void)call(post, 0x01E2475Au, 23u, 0u, 0x01E8A908u, 0u);
    (void)call(post, 0x02003904u, 0u, 0x01E8A826u, 2u, 0u);
    (void)call(post, 0x01E60776u, 0u, 0u, 0u, 0u);
    *post->module_11bc0 = 0x01EB1361u;
    /* Stock app+0x024E92 is just a tail wrapper for this ROM call. */
    (void)call(post, 0x02002CAEu, 0x00012940u, 0u, 0u, 0u);

    /* Stock's 0x021127B4(0x12F40,0,0xA4) is a zero fill. */
    for (index = 0u; index < 0xA4u; ++index)
        post->storage_12f40[index] = 0u;
    post->list_12f38[0] = 0x00012F38u;
    post->list_12f38[1] = 0x00012F38u;
    (void)call(post, 0x02002CAEu, 0x00012F38u, 0u, 0u, 0u);
    (void)call(post, 0x02002948u, 0x00012F90u, 0u, 0u, 0u);
    task_status = h9_main_task_launch(post->task_hooks,
                                      post->dev_mg_entry, 0u, "dev_mg");
    if (task_status != 0)
        /* Stock app+0x04A574 is a tail wrapper for this ROM call. */
        (void)call(post, 0x0200010Au, (uint32_t)task_status,
                   0u, 0u, 0u);
    (void)call(post, 0x02002B66u, 0x00012F90u, 0u, 0u, 0u);
    (void)call(post, 0x01E5176Eu, *post->state_12188, 0u, 0u, 0u);
    return true;
}
