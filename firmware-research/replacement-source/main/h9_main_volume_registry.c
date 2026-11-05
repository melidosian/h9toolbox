#include "h9_main_volume_registry.h"

#include <string.h>

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(H9MainVolumeNode) == 24u,
               "Pi32 volume node must match the stock 24-byte allocation");
#endif

const H9MainVolumeDriver h9_main_stock_volume_drivers[3] = {
    { "sdfile" }, { "fat_nor" }, { "res_nor" }
};

static bool same_name(const char *left, const char *right)
{
    unsigned index;
    if (left == NULL || right == NULL) return false;
    for (index = 0u; index < 32u; ++index) {
        if (left[index] != right[index]) return false;
        if (left[index] == '\0') return true;
    }
    return false;
}

bool h9_main_volume_registry_init(H9MainVolumeRegistry *registry,
                                   const H9MainVolumeDriver *drivers,
                                   size_t driver_count,
                                   const H9MainVolumeRegistryHooks *hooks)
{
    if (registry == NULL || drivers == NULL || driver_count == 0u ||
        hooks == NULL || hooks->lock == NULL || hooks->unlock == NULL ||
        hooks->allocate_node == NULL || hooks->open_driver == NULL)
        return false;
    memset(registry, 0, sizeof *registry);
    registry->drivers = drivers;
    registry->driver_count = driver_count;
    registry->hooks = *hooks;
    registry->head.next = &registry->head;
    registry->head.previous = &registry->head;
    return true;
}

H9MainVolumeStatus h9_main_volume_registry_register(
    H9MainVolumeRegistry *registry, const char *name)
{
    const H9MainVolumeDriver *driver = NULL;
    H9MainVolumeNode *node;
    H9MainVolumeLinks *link;
    uintptr_t handle;
    size_t index;
    if (registry == NULL || name == NULL || registry->head.next == NULL)
        return H9_MAIN_VOLUME_INVALID;
    for (index = 0u; index < registry->driver_count; ++index) {
        if (same_name(registry->drivers[index].name, name)) {
            driver = &registry->drivers[index];
            break;
        }
    }
    if (driver == NULL) return H9_MAIN_VOLUME_UNKNOWN;

    registry->hooks.lock(registry->hooks.context);
    for (link = registry->head.next; link != &registry->head;
         link = link->next) {
        node = (H9MainVolumeNode *)link;
        if (same_name(node->driver->name, name)) {
            registry->hooks.unlock(registry->hooks.context);
            return H9_MAIN_VOLUME_DUPLICATE;
        }
    }
    registry->hooks.unlock(registry->hooks.context);

    node = registry->hooks.allocate_node(registry->hooks.context);
    if (node == NULL) return H9_MAIN_VOLUME_NO_MEMORY;
    memset(node, 0, sizeof *node);
    registry->hooks.lock(registry->hooks.context);
    handle = registry->hooks.open_driver(registry->hooks.context, driver,
                                          3u, 0u);
    node->driver = driver;
    node->handle = handle;
    node->flags = handle != 0u ? 1u : 0u;
    node->sequence = registry->sequence++;
    node->links.next = &registry->head;
    node->links.previous = registry->head.previous;
    registry->head.previous->next = &node->links;
    registry->head.previous = &node->links;
    registry->hooks.unlock(registry->hooks.context);
    return handle != 0u ? H9_MAIN_VOLUME_OK : H9_MAIN_VOLUME_OPEN_FAILED;
}
