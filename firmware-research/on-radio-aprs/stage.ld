ENTRY(aprs_stage_entry)
SECTIONS
{
    .text 0x01EEF000 : { *(.text.entry) *(.text*) *(.rodata*) *(.srodata*) }
}
