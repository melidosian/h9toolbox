OUTPUT_FORMAT("elf32-pi32v2")
OUTPUT_ARCH(pi32v2)
ENTRY(hello_display_hook)
SECTIONS {
    . = 0x01EA78F0;
    .text : { *(.text .text.*) }
    /DISCARD/ : { *(.comment) *(.note*) }
}
