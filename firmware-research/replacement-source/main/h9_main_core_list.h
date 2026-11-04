#ifndef H9_MAIN_CORE_LIST_H
#define H9_MAIN_CORE_LIST_H

#include <stdint.h>

/* Stock app+0x0462F8 initializes a two-word self-linked RAM head at 0x119EC.
 * The list's consumers have not yet been mapped. */
typedef struct {
    volatile uint32_t *words;
    uint32_t self_address;
} H9MainCoreList;

void h9_main_core_list_bind_target(H9MainCoreList *list);
int h9_main_core_list_init(const H9MainCoreList *list);

#endif
