#include "h9_main_serial_commands.h"

#include <stddef.h>

#if defined(__PI32V2__)
#define H9_MAIN_RAM_CODE __attribute__((section(".ram_code.serial_commands"), noinline))
#else
#define H9_MAIN_RAM_CODE
#endif

H9_MAIN_RAM_CODE
static bool usable(const H9MainSerialCommands *commands)
{
    return commands != NULL && commands->control != NULL &&
           commands->begin != NULL && commands->end != NULL &&
           commands->exchange != NULL;
}

H9_MAIN_RAM_CODE
static void start_command(volatile uint32_t *control, uint32_t command)
{
    if ((command & 0x8000u) == 0u) *control &= ~0x100u;
    *control |= 0x101u;
}

H9_MAIN_RAM_CODE
static bool exchange(const H9MainSerialCommands *commands, uint32_t word,
                     uint8_t *received)
{
    uint8_t ignored;
    return commands->exchange(commands->context, word,
                              received != NULL ? received : &ignored);
}

H9_MAIN_RAM_CODE
bool h9_main_serial_command_read(const H9MainSerialCommands *commands,
                                  uint32_t command, uint8_t *received)
{
    bool ok;
    if (!usable(commands) || received == NULL ||
        !commands->begin(commands->context)) return false;
    start_command(commands->control, command);
    ok = exchange(commands, ((command >> 8) | 0x80u) & 0x83u, NULL) &&
         exchange(commands, command & 0xFFFF00FFu, NULL) &&
         exchange(commands, 0x5Eu, received);
    *commands->control &= ~0x101u;
    commands->end(commands->context);
    return ok;
}

H9_MAIN_RAM_CODE
bool h9_main_serial_command_write(const H9MainSerialCommands *commands,
                                   uint32_t command, uint32_t payload)
{
    bool ok;
    if (!usable(commands) || !commands->begin(commands->context))
        return false;
    start_command(commands->control, command);
    ok = exchange(commands, (command >> 8) & 3u, NULL) &&
         exchange(commands, command & 0xFFFF00FFu, NULL) &&
         exchange(commands, payload, NULL);
    *commands->control &= ~0x101u;
    commands->end(commands->context);
    return ok;
}
