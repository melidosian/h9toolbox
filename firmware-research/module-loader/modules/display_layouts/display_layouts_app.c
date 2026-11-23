#include "app_theme.h"
#include "display_layouts.h"

unsigned module_main(const struct h9_api *api, unsigned self)
{
    (void)self;
    if (api && api->version >= H9_DISPLAY_LAYOUTS_API &&
        api->display_layouts_magic == H9_DISPLAY_LAYOUTS_MAGIC && api->display_layouts)
        return api->display_layouts(api);

    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
    ui_title("DISPLAY LAYOUTS");
    gfx_label(UI_LEFT, 35, "UPDATE H9 TOOLBOX FIRMWARE", 154, 1, UI_TEXT, UI_BG);
    gfx_label(UI_LEFT, 50, "TO ENABLE DISPLAY LAYOUTS.", 154, 1, UI_TEXT, UI_BG);
    ui_footer("BACK RETURN");
    gfx_wait_release();
    while (gfx_wait_key() != H9_KEY_BACK) {}
    return 0;
}
