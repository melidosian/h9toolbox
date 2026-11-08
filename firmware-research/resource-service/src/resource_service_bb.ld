OUTPUT_FORMAT("elf32-pi32v2")
OUTPUT_ARCH(pi32v2)
ENTRY(resource_s2_read)
/* Merged Brick Breaker + S2 (COMPACT service). Each section fills part of a
 * User Manual QR-code margin left free by the unchanged Brick Breaker:
 *   .s2      app+0xA51A8 (QR A margin, 857 bytes)
 *   .tables  app+0xA7E50 (after the 1,628-byte game, 72 bytes)
 *   .svc     app+0xA9E00 (after the 94-byte launcher, 906 bytes) */
SECTIONS {
    . = 0x01EA52A8;
    .s2 : { *(.s2) }
    . = 0x01EA7F50;
    .tables : { *(.tables) }
    . = 0x01EA9F00;
    .svc : { *(.svc) }
    /DISCARD/ : { *(.comment) *(.note*) }
}
