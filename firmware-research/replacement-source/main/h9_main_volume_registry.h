#ifndef H9_MAIN_VOLUME_REGISTRY_H
#define H9_MAIN_VOLUME_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct H9MainVolumeLinks H9MainVolumeLinks;
struct H9MainVolumeLinks {
    H9MainVolumeLinks *next;
    H9MainVolumeLinks *previous;
};

/* On Pi32 this is the 24-byte record allocated by app+0x04A7B2. */
typedef struct {
    const char *name;
} H9MainVolumeDriver;

typedef struct {
    H9MainVolumeLinks links;
    const H9MainVolumeDriver *driver;
    uintptr_t handle;
    uint8_t flags;
    uint8_t reserved[3];
    uint32_t sequence;
} H9MainVolumeNode;

typedef struct {
    void (*lock)(void *context);
    void (*unlock)(void *context);
    H9MainVolumeNode *(*allocate_node)(void *context);
    uintptr_t (*open_driver)(void *context,
                             const H9MainVolumeDriver *driver,
                             uint8_t mode, uint32_t option);
    void *context;
} H9MainVolumeRegistryHooks;

typedef struct {
    H9MainVolumeLinks head;
    const H9MainVolumeDriver *drivers;
    size_t driver_count;
    H9MainVolumeRegistryHooks hooks;
    uint32_t sequence;
} H9MainVolumeRegistry;

typedef enum {
    H9_MAIN_VOLUME_OK = 0,
    H9_MAIN_VOLUME_DUPLICATE = 1,
    H9_MAIN_VOLUME_NO_MEMORY = 3,
    H9_MAIN_VOLUME_UNKNOWN = 4,
    H9_MAIN_VOLUME_OPEN_FAILED = 5,
    H9_MAIN_VOLUME_INVALID = -1
} H9MainVolumeStatus;

extern const H9MainVolumeDriver h9_main_stock_volume_drivers[3];

bool h9_main_volume_registry_init(H9MainVolumeRegistry *registry,
                                   const H9MainVolumeDriver *drivers,
                                   size_t driver_count,
                                   const H9MainVolumeRegistryHooks *hooks);
/* Mirrors the stock lookup, duplicate check, allocate, open and insertion.
 * An open failure still inserts a node with a null handle, as stock does. */
H9MainVolumeStatus h9_main_volume_registry_register(
    H9MainVolumeRegistry *registry, const char *name);

#endif
