OUTPUT_FORMAT("elf32-pi32v2")
OUTPUT_ARCH(pi32v2)
ENTRY(far_header)
/* Flash 0xEA000 (outside the settings VM), mapped at 0x01EE5000. */
SECTIONS {
    . = 0x01EE5000;
    .far_header : { *(.far_header) }
    .text : { *(.text .text.*) *(.rodata .rodata.*) }
    /DISCARD/ : { *(.comment) *(.note*) }
}
