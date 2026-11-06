/* Early-execution probe only. The actual board memory map is incomplete. */
OUTPUT_FORMAT("elf32-pi32v2")
OUTPUT_ARCH(pi32v2)
ENTRY(h9_main_reset)

MEMORY {
    CODE (rx) : ORIGIN = 0x01E00100, LENGTH = 0xD3C2C
    RAM (rw)  : ORIGIN = 0x0000E2E0, LENGTH = 0x101F4
}

SECTIONS {
    .text : {
        KEEP(*(.text.h9_reset))
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > CODE
    .data : { *(.data .data.*) } > RAM AT> CODE
    .bss (NOLOAD) : { *(.bss .bss.* COMMON) } > RAM
    __stock_clear_start = ORIGIN(RAM);
    __stock_clear_end = ORIGIN(RAM) + LENGTH(RAM);
    /DISCARD/ : { *(.comment) *(.note*) }
}

ASSERT(SIZEOF(.data) == 0, "boot probe must not need initialized RAM")
ASSERT(SIZEOF(.bss) == 0, "boot probe must not need BSS")
