#ifndef H9_MAIN_SERIAL_COMMANDS_H
#define H9_MAIN_SERIAL_COMMANDS_H

#include <stdbool.h>
#include <stdint.h>

/* Stock RAM 0x58/0xA0 transact through the byte exchange at RAM 0x40.
 * The begin/end callbacks stand for stock RAM 0x1634/0x1650, whose gate
 * and reference-count writes still need a board binding. */
typedef struct {
    volatile uint32_t *control;
    bool (*begin)(void *context);
    bool (*end)(void *context);
    bool (*exchange)(void *context, uint32_t word, uint8_t *received);
    void *context;
} H9MainSerialCommands;

bool h9_main_serial_command_read(const H9MainSerialCommands *commands,
                                  uint32_t command, uint8_t *received);
bool h9_main_serial_command_write(const H9MainSerialCommands *commands,
                                   uint32_t command, uint32_t payload);

#endif
