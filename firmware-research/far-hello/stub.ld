OUTPUT_FORMAT("elf32-pi32v2")
OUTPUT_ARCH(pi32v2)
ENTRY(far_stub)
brick_breaker_after_stock_setup = 0x01EA7942;
SECTIONS {
    . = 0x01EA7F80;
    .text : { *(.text) }
    /DISCARD/ : { *(.comment) *(.note*) }
}
