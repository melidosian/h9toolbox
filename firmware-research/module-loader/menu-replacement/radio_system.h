#ifndef RADIO_SYSTEM_H
#define RADIO_SYSTEM_H
#include "radio_settings.h"
#include "h9mod.h"

enum radio_system_action {
    RADIO_SYS_INFO, RADIO_SYS_APRS_INFO, RADIO_SYS_MANUAL,
    RADIO_SYS_RESET_VFO, RADIO_SYS_RESET_CHANNELS, RADIO_SYS_RESET_ANALOG,
    RADIO_SYS_RESET_ALL, RADIO_SYS_RESET_APRS
};
struct radio_version_parser {
    uint8_t bytes[36], count, escaped, overflow;
    char module[16], protocol[16];
};
int radio_version_feed(struct radio_version_parser *, unsigned byte);
int radio_system(const struct h9_api *, unsigned action);
void radio_manual_read(unsigned page, unsigned offset, uint8_t *dst, unsigned count);
int radio_apply_pending(const struct h9_api *);
typedef int (*radio_rf_action)(void *);
int radio_with_rf(const struct h9_api *, radio_rf_action, void *);

struct radio_system_identity { unsigned region; char base[16], hardware[16], ani[4]; };
struct radio_system_platform {
    void (*identity)(struct radio_system_identity *);
    int (*reload)(void);
    int (*reset)(unsigned);
    void (*query_version)(void);
    unsigned (*ring_end)(void);
    unsigned (*ring_byte)(unsigned);
    unsigned (*ticks)(void);
};
extern const struct radio_system_platform radio_system_platform;
#endif
