/* Power-cut-safe APRS mailbox snapshots in two reserved resource sectors.
 * The caller supplies the same guarded NOR I/O used by Toolbox settings. */
#ifndef H9_MAIL_PAIR_H
#define H9_MAIL_PAIR_H

#include "toolbox_settings_pair.h"

#define MAIL_SECTOR_A 0x3FA000u
#define MAIL_SECTOR_B 0x3FB000u
#define MAIL_GAME_DIRECTORY 0xA1F000u
#define MAIL_ITEMS 6u

struct mail_entry {
    char from[10], to[10], text[40];
    uint8_t flags, id;             /* bit 0: addressed to me; bit 1: ACK; bit 2: REJ */
};

struct mail_snapshot {
    char owner[10];               /* own callsign, including SSID */
    uint8_t count, next_id;
    struct mail_entry item[MAIL_ITEMS];
};

enum mail_result {
    MAIL_OK, MAIL_EMPTY, MAIL_CLAIMED, MAIL_FOREIGN, MAIL_OWNER,
    MAIL_DAMAGED, MAIL_IO, MAIL_INVALID, MAIL_EXHAUSTED, MAIL_RESTARTABLE
};

enum mail_result mail_load(const struct tsp_io *io, struct mail_snapshot *out);
enum mail_result mail_save(const struct tsp_io *io, const struct mail_snapshot *next);

#endif
