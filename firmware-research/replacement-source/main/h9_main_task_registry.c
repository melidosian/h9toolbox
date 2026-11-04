#include "h9_main_task_registry.h"

/* Stock table order is retained. The board entry first requests app_core,
 * then my_app; creating those tasks requires their replacement entrypoints
 * and the still-unmapped ROM task-creation boundary. */
const H9MainTaskDescriptor h9_main_tasks[H9_MAIN_TASK_COUNT] = {
    { "app_core",         1u, 0x0380u, 0x0300u },
    { "my_app",           2u, 0x0800u, 0x0300u },
    { "usb_wr_freq_task", 4u, 0x0200u, 0x0000u },
    { "my_ui",            3u, 0x0400u, 0x0080u },
    { "my_task_debug_loop", 1u, 0x0400u, 0x0000u },
    { "sys_event",        6u, 0x0100u, 0x0000u },
    { "btctrler",         4u, 0x0200u, 0x0180u },
    { "btencry",          1u, 0x0200u, 0x0080u },
    { "btstack",          3u, 0x0600u, 0x0100u },
    { "audio_dec",        3u, 0x0320u, 0x0080u },
    { "audio_enc",        3u, 0x0200u, 0x0080u },
    { "aec",              2u, 0x0300u, 0x0000u },
    { "aec_dbg",          3u, 0x0080u, 0x0080u },
    { "update",           1u, 0x0200u, 0x0000u },
    { "dw_update",        2u, 0x0100u, 0x0080u },
    { "systimer",         6u, 0x0080u, 0x0000u },
    { "dev_mg",           3u, 0x0200u, 0x0200u },
    { "usb_msd",          1u, 0x0200u, 0x0080u },
    { "usb_audio",        5u, 0x0100u, 0x0100u },
    { "plnk_dbg",         5u, 0x0100u, 0x0100u },
    { "adc_linein",       2u, 0x0300u, 0x0080u },
    { "fm_task",          3u, 0x0200u, 0x0080u },
    { "enc_write",        1u, 0x0300u, 0x0000u },
    { "rcsp_task",        4u, 0x0300u, 0x0080u },
    { "ui",               3u, 0x0140u, 0x0080u },
    { "key_tone",         5u, 0x0100u, 0x0020u },
    { "mic_stream",       5u, 0x0300u, 0x0080u },
};

static int names_equal(const char *left, const char *right)
{
    if (left == NULL || right == NULL) return 0;
    while (*left != '\0' && *left == *right) {
        ++left;
        ++right;
    }
    return *left == *right;
}

const H9MainTaskDescriptor *h9_main_task_find(const char *name)
{
    size_t index;
    if (name == NULL) return NULL;
    for (index = 0u; index < H9_MAIN_TASK_COUNT; ++index)
        if (names_equal(h9_main_tasks[index].name, name))
            return &h9_main_tasks[index];
    return NULL;
}

int h9_main_task_launch(const H9MainTaskHooks *hooks,
                        uintptr_t entry, uintptr_t argument,
                        const char *name)
{
    const H9MainTaskDescriptor *descriptor = h9_main_task_find(name);
    if (descriptor != NULL && hooks != NULL && hooks->create != NULL &&
        hooks->create(hooks->context, entry, argument,
                      descriptor->byte_4, descriptor->halfword_6,
                      descriptor->halfword_8, name) == 0)
        return 0;
    if (hooks != NULL && hooks->report_failure != NULL)
        hooks->report_failure(hooks->context);
    return H9_MAIN_TASK_LAUNCH_ERROR;
}

static int target_create(void *context, uintptr_t entry, uintptr_t argument,
                         uint8_t byte_4, uint16_t halfword_6,
                         uint16_t halfword_8, const char *name)
{
    typedef int (*RomCreate)(uintptr_t, uintptr_t, uint32_t, uint32_t,
                             uint32_t, const char *);
    (void)context;
    return ((RomCreate)(uintptr_t)0x02003102u)(
        entry, argument, byte_4, halfword_6, halfword_8, name);
}

void h9_main_task_hooks_bind_target(H9MainTaskHooks *hooks)
{
    if (hooks == NULL) return;
    hooks->context = NULL;
    hooks->create = target_create;
    hooks->report_failure = NULL;
}
