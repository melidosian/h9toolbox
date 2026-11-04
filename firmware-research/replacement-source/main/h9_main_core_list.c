#include "h9_main_core_list.h"

#include <stddef.h>

void h9_main_core_list_bind_target(H9MainCoreList *list)
{
    if (list == NULL) return;
    list->words = (volatile uint32_t *)(uintptr_t)0x000119ECu;
    list->self_address = 0x000119ECu;
}

int h9_main_core_list_init(const H9MainCoreList *list)
{
    if (list == NULL || list->words == NULL) return -1;
    list->words[0] = list->self_address;
    list->words[1] = list->self_address;
    return 0;
}
