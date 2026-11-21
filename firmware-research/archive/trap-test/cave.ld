OUTPUT_FORMAT("elf32-pi32v2")
OUTPUT_ARCH(pi32v2)
ENTRY(hello_display_hook)
/* Startup in the app cave (app+0xA77F0, 1,704 bytes; runs at boot, entered
 * by a `call` at my_app's entry), everything else in the far region (flash
 * 0xEA000 = 0x01EE5000, stored plain; runs when called after the startup,
 * probe 2, 2026-11-09). */
SECTIONS {
    . = 0x01EA78F0;
    .cave : { *(.cave) }
    . = 0x01EE5000;
    .far_header : { *(.far_header) }
    .text : { *(.text .text.*) *(.rodata .rodata.*) }
    /DISCARD/ : { *(.comment) *(.note*) }
}
