#ifndef RADIO_BLUETOOTH_H
#define RADIO_BLUETOOTH_H
#include "radio_settings.h"

/* Virtual setting offsets, routed by radio_stock_storage. These two controls
 * apply immediately and must not cause the menu's settings-reload restart. */
enum { RADIO_BT_MIC = 0x6000, RADIO_BT_SPEAKER = 0x6001 };
int radio_bt_read(unsigned offset, void *dst, unsigned length);
int radio_bt_write(unsigned offset, const void *src, unsigned length);
int radio_bt_info(void);
int radio_bt_pairing(void);
int radio_bt_reset(void); /* caller supplies the reset confirmation */

struct radio_bt_device { char name[40]; uint8_t address[6]; };
/* Platform boundary lets the host harness exercise the same screens and
 * transaction flow without executing firmware addresses or writing a radio. */
struct radio_bt_platform {
    int (*identity)(char name[33]);
    unsigned (*internal)(unsigned speaker);
    int (*set_internal)(unsigned speaker, unsigned on);
    int (*open)(unsigned saved);
    unsigned (*count)(void);
    int (*device)(unsigned index, struct radio_bt_device *device);
    int (*connect)(unsigned index);
    unsigned (*connected)(void);
    void (*close)(void);
    int (*reset)(void);
};
extern const struct radio_bt_platform radio_bt_platform;
#endif
