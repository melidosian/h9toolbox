OUTPUT_FORMAT("elf32-pi32v2")
OUTPUT_ARCH(pi32v2)
ENTRY(brick_breaker_hotkey)
SECTIONS {
    . = 0x01EA78F0;
    .text : { *(.text .text.*) }
    . = 0x01EA9E9E;
    .launcher : { *(.launcher) }
    /DISCARD/ : { *(.comment) *(.note*) }
}
