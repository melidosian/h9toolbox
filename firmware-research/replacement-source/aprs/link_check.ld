/* Diagnostic link envelope from APRS-1.0.15.bin, not a validated memory map.
   No update binary is produced by the link-check tool. */
ENTRY(Reset_Handler)

MEMORY {
    FLASH (rx)  : ORIGIN = 0x08002000, LENGTH = 0x4398
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 0x1A50
}

__stack_top = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS {
    .isr_vector ORIGIN(FLASH) : {
        KEEP(*(.isr_vector))
    } > FLASH

    .text : {
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > FLASH

    .ARM.exidx : {
        *(.ARM.exidx .ARM.exidx.*)
        . = ALIGN(4);
    } > FLASH

    .data ORIGIN(RAM) + 0xC0 : {
        __data_start = .;
        *(.data .data.*)
        . = ALIGN(4);
        __data_end = .;
    } > RAM AT> FLASH
    __data_load_start = LOADADDR(.data);

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > RAM

    /DISCARD/ : { *(.comment) *(.note*) }
}

ASSERT(SIZEOF(.isr_vector) == 192, "Expected 48 Cortex vectors")
ASSERT(LOADADDR(.data) + SIZEOF(.data) <=
       ORIGIN(FLASH) + LENGTH(FLASH), "APRS source exceeds stock image length")
ASSERT(__bss_end + 0x400 <= __stack_top,
       "Less than 1 KiB remains for diagnostic stack")
