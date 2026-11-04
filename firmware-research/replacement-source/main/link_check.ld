/* Diagnostic only. Code base/length follow the extracted v1.0.38 app entry;
 * the RAM range is a provisional container for link analysis, not a verified
 * TD-H9 memory map. Do not package or flash this ELF. */
OUTPUT_FORMAT("elf32-pi32v2")
OUTPUT_ARCH(pi32v2)
ENTRY(h9_link_check_entry)

MEMORY {
    CODE (rx) : ORIGIN = 0x01E00100, LENGTH = 0xD3C2C
    RAM (rw)  : ORIGIN = 0x0000C000, LENGTH = 0x44000
}

SECTIONS {
    .text : {
        KEEP(*(.text.h9_link_check_entry))
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > CODE

    .data : {
        *(.data .data.*)
        . = ALIGN(4);
    } > RAM AT> CODE

    .bss (NOLOAD) : {
        *(.bss .bss.* COMMON)
        . = ALIGN(4);
    } > RAM

    /DISCARD/ : { *(.comment) *(.note*) }
}

ASSERT(LOADADDR(.data) + SIZEOF(.data) <=
       ORIGIN(CODE) + LENGTH(CODE), "main source exceeds stock app size")
