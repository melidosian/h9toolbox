#ifndef H9_RESIDENT_DISPATCH_H
#define H9_RESIDENT_DISPATCH_H
#include <stdint.h>
/* Cooperative only: one task, never ISR. Not a mutex or stock RF lock. */
#define H9_RS_MAGIC UINT32_C(0x48525331)
#define H9_RS_ABI 1u
#define H9_RS_COOPERATIVE 1u
#define H9_RS_PERMANENT 2u
#define H9_RS_AUDITED 4u
#define H9_RS_REQUIRED (H9_RS_COOPERATIVE | H9_RS_PERMANENT | H9_RS_AUDITED)
enum h9_rs_result { H9_RS_OK, H9_RS_INVALID, H9_RS_REENTRANT,
    H9_RS_SUPPRESSED, H9_RS_RF_BUSY, H9_RS_FAULT };
enum h9_rs_owner { H9_RS_NONE, H9_RS_MODULE, H9_RS_SERVICE };
struct h9_rs_state {
    uint32_t magic, generation, lease;
    uint16_t suppression;
    uint8_t active, fault, owner;
};
/* Callback must not transmit. Ownership below only models cooperative callers. */
struct h9_rs_binding {
    uint32_t magic;
    uint16_t abi, size;
    uint32_t capabilities;
    void (*poll)(void *context, uint32_t tick);
    void *context;
};
void h9_rs_init(struct h9_rs_state *s);
enum h9_rs_result h9_rs_poll(struct h9_rs_state *s,
    const struct h9_rs_binding *b, uint32_t caller_caps, uint32_t tick);
enum h9_rs_result h9_rs_suppress(struct h9_rs_state *s);
enum h9_rs_result h9_rs_resume(struct h9_rs_state *s);
enum h9_rs_result h9_rs_acquire(struct h9_rs_state *s,
    enum h9_rs_owner owner, uint32_t *lease);
enum h9_rs_result h9_rs_release(struct h9_rs_state *s,
    enum h9_rs_owner owner, uint32_t lease);
#endif
