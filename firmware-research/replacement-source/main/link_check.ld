/* Diagnostic only. Code base/length follow the extracted v1.0.38 app entry;
 * RAM 0xE2E0..0x1E4D4 follows the observed stock zeroed interval. This is
 * not a complete board memory map. Do not package or flash this ELF. */
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

    .data : {
        __data_start = .;
        *(.data .data.*)
        . = ALIGN(4);
        __data_end = .;
    } > RAM AT> CODE

    __data_load_start = LOADADDR(.data);

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > RAM

    /DISCARD/ : { *(.comment) *(.note*) }
}

ASSERT(LOADADDR(.data) + SIZEOF(.data) <=
       ORIGIN(CODE) + LENGTH(CODE), "main source exceeds stock app size")
