ENTRY(aprs_stage_entry)
SECTIONS
{
    .text 0x01EF0000 : { *(.text.entry) *(.text*) *(.rodata*) *(.srodata*) }
}
