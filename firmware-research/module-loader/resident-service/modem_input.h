#ifndef H9_BEACON_MODEM_INPUT_H
#define H9_BEACON_MODEM_INPUT_H
#include "beacon_service.h"

enum { H9_MODEM_DMA_CAPACITY = 2048, H9_MODEM_DMA_CHUNK = 1024, H9_MODEM_POLL_BYTES = 128 };
struct h9_modem_cursor {
    uint32_t next;
    uint8_t attached;
};

/* Poll an independent read cursor. The stock DMA consumer is untouched.
 * With an unpublished chunk in progress, only the latest 1024 published
 * bytes are safe to copy. A NULL buffer detaches until the UART returns. */
void h9_modem_poll(struct h9_modem_cursor *, struct h9_bs *,
                   const volatile uint8_t *buffer,
                   const volatile uint32_t *produced, uint32_t now_ms);
#endif
