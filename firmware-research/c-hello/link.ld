OUTPUT_FORMAT("elf32-pi32v2")
OUTPUT_ARCH(pi32v2)
ENTRY(hello_display_hook)
/* The erased cave stage 23 used (app+0xA77F0, 1,704 bytes). entry.o is linked
 * first so its hook entry starts the cave. */
SECTIONS {
    . = 0x01EA78F0;
    .text : { *(.text .text.*) *(.rodata .rodata.*) }
    /DISCARD/ : { *(.comment) *(.note*) }
}
