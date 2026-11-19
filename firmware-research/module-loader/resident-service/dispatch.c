#include "dispatch.h"
#include <stddef.h>
static enum h9_rs_result valid(const struct h9_rs_state *s)
{
    if (!s || s->magic != H9_RS_MAGIC) return H9_RS_INVALID;
    if (s->fault || s->owner > H9_RS_SERVICE) return H9_RS_FAULT;
    return H9_RS_OK;
}
static enum h9_rs_result fail(struct h9_rs_state *s)
{
    s->fault = 1;
    return H9_RS_FAULT;
}
void h9_rs_init(struct h9_rs_state *s)
{
    if (s) {
        s->magic = H9_RS_MAGIC;
        s->generation = s->lease = 0;
        s->suppression = 0;
        s->active = s->fault = 0;
        s->owner = H9_RS_NONE;
    }
}
enum h9_rs_result h9_rs_poll(struct h9_rs_state *s,
    const struct h9_rs_binding *b, uint32_t caps, uint32_t tick)
{
    enum h9_rs_result r = valid(s);
    if (r != H9_RS_OK) return r;
    if (!b || b->magic != H9_RS_MAGIC || b->abi != H9_RS_ABI ||
        b->size != sizeof(*b) || !b->poll ||
        (b->capabilities & H9_RS_REQUIRED) != H9_RS_REQUIRED ||
        (caps & H9_RS_REQUIRED) != H9_RS_REQUIRED) return H9_RS_INVALID;
    if (s->active) return H9_RS_REENTRANT;
    if (s->suppression) return H9_RS_SUPPRESSED;
    if (s->owner != H9_RS_NONE) return H9_RS_RF_BUSY;
    s->active = 1;
    b->poll(b->context, tick);
    s->active = 0;
    /* An unbalanced callback must never silently unlock the radio. */
    if (s->owner != H9_RS_NONE || s->suppression) return fail(s);
    return valid(s);
}
enum h9_rs_result h9_rs_suppress(struct h9_rs_state *s)
{
    enum h9_rs_result r = valid(s);
    if (r != H9_RS_OK) return r;
    if (s->suppression == UINT16_MAX) return fail(s);
    ++s->suppression;
    return H9_RS_OK;
}
enum h9_rs_result h9_rs_resume(struct h9_rs_state *s)
{
    enum h9_rs_result r = valid(s);
    if (r != H9_RS_OK) return r;
    if (!s->suppression) return fail(s);
    --s->suppression;
    return H9_RS_OK;
}
enum h9_rs_result h9_rs_acquire(struct h9_rs_state *s,
    enum h9_rs_owner owner, uint32_t *lease)
{
    enum h9_rs_result r = valid(s);
    if (lease) *lease = 0;
    if (r != H9_RS_OK) return r;
    if (!lease || (owner != H9_RS_MODULE && owner != H9_RS_SERVICE))
        return H9_RS_INVALID;
    if ((owner == H9_RS_SERVICE) != (s->active != 0)) return H9_RS_INVALID;
    if (s->suppression) return H9_RS_SUPPRESSED;
    if (s->owner != H9_RS_NONE) return H9_RS_RF_BUSY;
    if (s->generation == UINT32_MAX) return fail(s);
    s->lease = ++s->generation;
    s->owner = (uint8_t)owner;
    *lease = s->lease;
    return H9_RS_OK;
}
enum h9_rs_result h9_rs_release(struct h9_rs_state *s,
    enum h9_rs_owner owner, uint32_t lease)
{
    enum h9_rs_result r = valid(s);
    if (r != H9_RS_OK) return r;
    if (!lease || owner == H9_RS_NONE || s->owner != owner || s->lease != lease)
        return fail(s);
    s->owner = H9_RS_NONE;
    s->lease = 0;
    return H9_RS_OK;
}
