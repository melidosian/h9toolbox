/* Hidden mailbox worker. Screen modules swap out before NOR operations so
 * the journal and directory verifier do not consume their code budget. */
#include "aprs_core.h"
#include "../common/mail_pair.h"

#define NOR_READY (*(const volatile uint16_t *)0x12234u == 0x5AA5u)

static int read_nor(void *context, uint32_t address, void *dst, unsigned size)
{
    if (!NOR_READY) return 0;
    ((const struct h9_api *)context)->read(dst, address, size);
    return 1;
}
static int write_nor(void *context, uint32_t address, const void *src, unsigned size)
{
    (void)context;
    if (!NOR_READY || !((address >= MAIL_SECTOR_A && address < MAIL_SECTOR_A + 416 &&
                        !(address & 31u) && size == 32 && address + size <= MAIL_SECTOR_A + 416) ||
                       (address >= MAIL_SECTOR_B && address < MAIL_SECTOR_B + 416 &&
                        !(address & 31u) && size == 32 && address + size <= MAIL_SECTOR_B + 416) ||
                       ((address == MAIL_SECTOR_A + 444 || address == MAIL_SECTOR_B + 444) && size == 4))) return 0;
    ((void (*)(const void *, uint32_t, unsigned))0x01E5006Eu)(src, address, size);
    return 1;
}
static int erase_nor(void *context, uint32_t address)
{
    (void)context;
    if (!NOR_READY || (address != MAIL_SECTOR_A && address != MAIL_SECTOR_B)) return 0;
    ((void (*)(uint32_t))0x01E50000u)(address >> 12);
    return 1;
}
static void copy(char *dst, const char *src, unsigned size)
{
    unsigned i = 0;
    for (; i + 1 < size && src[i]; ++i) dst[i] = src[i];
    dst[i] = 0;
    for (++i; i < size; ++i) dst[i] = 0;
}
static int same(const char *a, const char *b)
{
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}
static void restore(const struct mail_snapshot *s)
{
    A->msgs = s->count;
    A->msg_id = s->next_id;
    for (unsigned i = 0; i < s->count; ++i) {
        struct message *m = &A->msg[i];
        const struct mail_entry *e = &s->item[i];
        copy(m->from, e->from, CALL);
        copy(m->to, e->to, CALL);
        copy(m->text, e->text, MSG_TEXT);
        m->mine = (e->flags & 1u) != 0;
        m->acked = (e->flags & 2u) != 0;
        m->rejected = (e->flags & 4u) != 0;
        m->live = 1;
        m->id = e->id;
    }
    if (A->msg_sel >= A->msgs) A->msg_sel = 0;
}
static void snapshot(struct mail_snapshot *s)
{
    copy(s->owner, A->mycall, sizeof s->owner);
    s->count = A->msgs;
    s->next_id = A->msg_id;
    for (unsigned i = 0; i < s->count; ++i) {
        struct mail_entry *e = &s->item[i];
        const struct message *m = &A->msg[i];
        copy(e->from, m->from, sizeof e->from);
        copy(e->to, m->to, sizeof e->to);
        copy(e->text, m->text, sizeof e->text);
        e->flags = (m->mine ? 1u : 0u) | (m->acked ? 2u : 0u) | (m->rejected ? 4u : 0u);
        e->id = m->id;
    }
}

unsigned module_main(const struct h9_api *api, unsigned self)
{
    (void)self;
    static const char *const PAGE[] = { APRS_LIST, APRS_INFO, APRS_MAP, APRS_MSG, APRS_SEND };
    struct tsp_io io = { .ctx = (void *)api, .read = read_nor, .program = write_nor,
                         .erase = erase_nor, .sector = { MAIL_SECTOR_A, MAIL_SECTOR_B },
                         .ownership_approved = 1 };
    struct mail_snapshot s = { .count = 0 };
    aprs_open();
    enum mail_result result = MAIL_INVALID;
    if (A->mycall[0] && NOR_READY) {
        if (A->mail_action == MAIL_OP_LOAD) {
            result = mail_load(&io, &s);
            if (result == MAIL_OK && same(s.owner, A->mycall)) restore(&s);
            else if (result == MAIL_EMPTY) A->mail_dirty = A->msgs != 0;
            else if (result == MAIL_RESTARTABLE && same(s.owner, A->mycall))
                A->mail_dirty = A->msgs != 0;
            else if (result == MAIL_RESTARTABLE) result = MAIL_OWNER;
            else if (result == MAIL_OK) result = MAIL_OWNER;
        } else if (A->mail_action == MAIL_OP_SAVE) {
            snapshot(&s);
            result = mail_save(&io, &s);
        }
    }
    A->mail_status = result == MAIL_OK || result == MAIL_EMPTY || result == MAIL_RESTARTABLE ? MAIL_READY :
                     result == MAIL_CLAIMED ? MAIL_BLOCKED :
                     result == MAIL_OWNER ? MAIL_OTHER_OWNER : MAIL_UNAVAILABLE;
    if (A->mail_status != MAIL_READY || A->mail_action == MAIL_OP_SAVE)
        A->mail_dirty = 0; /* avoid a switch loop if the sector is unavailable */
    A->mail_action = MAIL_OP_NONE;
    A->dirty = 1;
    aprs_go(api, A->mail_return < 5 ? PAGE[A->mail_return] : APRS_LIST);
    return 0;
}
