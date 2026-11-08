OUTPUT_FORMAT("elf32-pi32v2")
OUTPUT_ARCH(pi32v2)
ENTRY(entry)
SECTIONS {
    . = 0x00012000;
    .text : { *(.text .text.*) }
    /DISCARD/ : { *(.comment) *(.note*) }
}
