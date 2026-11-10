/* H9 Toolbox module loader: the main-flash stub (offline, not yet flashed).
 *
 * Replaces Brick Breaker in H9 Toolbox Firmware. It lives in the cave the
 * game used (app+0xA77F0, before the S2 tables at app+0xA7E50) and is entered
 * from the same my_app loop hook (app+0x088906) Brick Breaker's launcher used.
 *
 * Hold Back about 0.4 s (40 stock hold ticks, as Brick Breaker):
 *   1. ml_check validates the module bundle on the resource chip while the
 *      stock my_app frame is still intact. No valid bundle: the stock loop
 *      continues as if nothing happened (the radio stays usable).
 *   2. Otherwise drop my_app's frame (as Brick Breaker did) and enter
 *      ml_main, which never returns to the stock UI:
 *        run module 0 (the menu) -> it returns the index to run, or 0
 *        run that module         -> when it returns, back to the menu
 *        menu returns 0, or the menu cannot load -> reset (stock cpu_reset)
 *
 * Storage: the "modules" upload slot (dist/doom/upload.mjs, H9DATA01
 * directory at 0xCC0000, one segment at 0xCC1000, COMPLETE marker), holding
 * one bundle:
 *   +0  "H9MODS01"  +8 u16 version 1  +10 u16 count  +12 u32 bundle bytes
 *   +16 + 32*i: name[16], u32 offset (from bundle start), u32 size,
 *              u32 bss, u16 entry (from the load address), u16 CRC-16/XMODEM
 * Each module body is linked for, and copied to, RAM 0x29F00-0x2BEFF (8 KiB
 * taken from the stock heap: app+0x05FA9E, 0x2BF00 -> 0x29F00). The stub
 * zeroes bss, checks the body CRC in RAM, then calls
 *   entry(r0 = &ml_api, r1 = own index)
 * with the JieLi clang ABI (r4-r14 preserved by the module).
 */
    .section .text,"ax",@progbits

/* ---- my_app loop hook (Brick Breaker's launcher, retargeted) ----------
 * sp+68 in the original my_app frame holds an initialization-only temporary
 * (see brick-breaker/src/launcher.s). After saving rets/r4 it is at sp+76:
 * magic upper half 0xA5A5, stock hold count at arming below it. */
    .globl ml_hotkey
ml_hotkey:
    [--sp] = {rets, r4}
    r3 = 0x0000c3af
    r0 = b[r3+0] (u)
    if (r0 != 15) goto ml_back_released

    r2 = [sp+76]
    r1 = r2 >> 16
    r3 = 0xa5a5
    if (r1 != r3) goto ml_arm_back_hold
    r3 = 0x0000c3b2
    r0 = h[r3+0] (u)
    r2 = r2.l (u)
    if (r0 < r2) goto ml_arm_back_hold
    r0 -= r2
    if (r0 < 40) goto ml_stock_loop_call

    /* Held long enough. With no valid bundle, re-arm: the stock UI keeps
     * running and another 0.4 s of holding tries again. */
    call ml_check
    if (r0 == 0) goto ml_arm_back_hold

    /* my_app has only its original 124-byte task frame at this call site. */
    {rets, r4} = [sp++]
    sp += 124
    r3 = ml_main
    goto r3

ml_arm_back_hold:
    r3 = 0x0000c3b2
    r0 = h[r3+0] (u)
    r3 = 0xa5a50000
    r0 |= r3
    [sp+76] = r0
    goto ml_stock_loop_call

ml_back_released:
    r0 = 0
    [sp+76] = r0

ml_stock_loop_call:
    r3 = 0x01e58ee4
    call r3
    {pc, r4} = [sp++]

/* ---- run loop (never returns) ----------------------------------------- */
ml_main:
    call ml_check
    r10 = r0
    if (r10 == 0) goto ml_reset
    r11 = 0
ml_run:
    r0 = r11
    r1 = r10
    call ml_load
    if (r0 == 0) goto ml_load_failed
    r3 = r0
    r0 = ml_api
    r1 = r11
    call r3
    if (r11 != 0) goto ml_back_to_menu
    /* The menu returned: 0 = back to the radio, else the module to run. */
    if (r0 == 0) goto ml_reset
    r11 = r0
    goto ml_run
ml_load_failed:
    if (r11 == 0) goto ml_reset
ml_back_to_menu:
    r11 = 0
    goto ml_run

/* Restart the radio the way stock's cpu_reset does (app+0xC6EC8/0xC6EF4):
 * interrupts off (ROM 0x02001634), PWR_CON 0x1E0000 |= bit 4, spin. Same
 * sequence as h9_reset in modules/common/h9lib.s, proven on hardware by the
 * menu's RADIO item 2026-11-10. (ROM 0x0200000A froze when jumped to.) */
ml_reset:
    r3 = 0x02001634
    call r3
    r0 = 0x001e0000
    [r0+0] |= 0x10
ml_reset_wait:
    goto ml_reset_wait

/* ---- unsigned ml_check(void): module count of a valid bundle, else 0 --- */
ml_check:
    [--sp] = {rets, r7-r4}
    sp += -128
    r0 = 0x00012234
    r0 = h[r0+0] (u)
    r1 = 0x5aa5
    if (r0 != r1) goto mlc_fail

    /* Directory: H9DATA01, one segment at the slot's data start, CRC-16. */
    r0 = sp + 0
    r1 = 0x00cc0000
    r2 = 128
    call ml_read
    r0 = [sp+0]
    r1 = 0x41443948
    if (r0 != r1) goto mlc_fail
    r0 = [sp+4]
    r1 = 0x31304154
    if (r0 != r1) goto mlc_fail
    r0 = [sp+80]
    if (r0 != 1) goto mlc_fail
    r0 = [sp+84]
    r1 = 0x00cc1000
    if (r0 != r1) goto mlc_fail
    r4 = [sp+88]
    r0 = r4 >> 17
    if (r0 != 0) goto mlc_fail
    r0 = sp + 0
    r1 = 92
    call ml_crc
    r1 = sp + 0
    r1 = h[r1+92] (u)
    if (r0 != r1) goto mlc_fail

    /* Upload finished and verified: "H9DATA-COMPL..." in the last block. */
    r0 = sp + 0
    r1 = 0x00cc0fe0
    r2 = 16
    call ml_read
    r0 = [sp+0]
    r1 = 0x41443948
    if (r0 != r1) goto mlc_fail
    r0 = [sp+4]
    r1 = 0x432d4154
    if (r0 != r1) goto mlc_fail
    r0 = [sp+8]
    r1 = 0x4c504d4f
    if (r0 != r1) goto mlc_fail

    /* Bundle header: H9MODS01, version 1, count 1-255, length = upload. */
    r0 = sp + 0
    r1 = 0x00cc1000
    r2 = 16
    call ml_read
    r0 = [sp+0]
    r1 = 0x4f4d3948
    if (r0 != r1) goto mlc_fail
    r0 = [sp+4]
    r1 = 0x31305344
    if (r0 != r1) goto mlc_fail
    r1 = sp + 0
    r0 = h[r1+8] (u)
    if (r0 != 1) goto mlc_fail
    r5 = h[r1+10] (u)
    if (r5 == 0) goto mlc_fail
    r0 = r5 >> 8
    if (r0 != 0) goto mlc_fail
    r0 = [sp+12]
    if (r0 != r4) goto mlc_fail
    r0 = r5
    goto mlc_done
mlc_fail:
    r0 = 0
mlc_done:
    sp += 128
    {pc, r7-r4} = [sp++]

/* ---- entry ml_load(index, count): load module `index`, return its entry
 * address, or 0 if the index, the entry or the body CRC is wrong. -------- */
ml_load:
    [--sp] = {rets, r9-r4}
    sp += -32
    r2 = r0 >> 8
    if (r2 != 0) goto mll_fail
    if (r0 < r1) goto mll_index_ok
    goto mll_fail
mll_index_ok:
    r1 = r0 << 5
    r0 = 0x00cc1010
    r1 = r1 + r0
    r0 = sp + 0
    r2 = 32
    call ml_read
    r5 = [sp+16]
    r6 = [sp+20]
    r7 = [sp+24]
    r1 = sp + 0
    r8 = h[r1+28] (u)
    r9 = h[r1+30] (u)

    /* size 1..8 KiB, bss < 8 KiB, size + bss <= 8 KiB, entry < size,
     * offset + size inside the slot's 124 KiB. */
    if (r6 == 0) goto mll_fail
    r0 = r6 >> 13
    if (r0 != 0) goto mll_fail
    r0 = r7 >> 13
    if (r0 != 0) goto mll_fail
    r0 = r6 + r7
    r1 = 0x2000
    if (r1 < r0) goto mll_fail
    if (r8 < r6) goto mll_entry_ok
    goto mll_fail
mll_entry_ok:
    r0 = r5 >> 17
    if (r0 != 0) goto mll_fail
    r0 = r5 + r6
    r1 = 0x1f000
    if (r1 < r0) goto mll_fail

    r0 = 0x00029f00
    r1 = 0x00cc1000
    r1 = r1 + r5
    r2 = r6
    call ml_read

    r0 = 0x00029f00
    r0 = r0 + r6
    r1 = 0
mll_zero:
    if (r7 == 0) goto mll_zero_done
    b[r0+0] = r1
    r0 += 1
    r7 += -1
    goto mll_zero
mll_zero_done:
    r0 = 0x00029f00
    r1 = r6
    call ml_crc
    if (r0 != r9) goto mll_fail
    csync
    r0 = 0x00029f00
    r0 = r0 + r8
    goto mll_done
mll_fail:
    r0 = 0
mll_done:
    sp += 32
    {pc, r9-r4} = [sp++]

/* ---- void ml_read(dst, chip_address, length): stock SPI2 NOR read
 * 0x01E4FF22 (ROM lock, CS GPIO 23, command 03; DMA when dst is 4-byte
 * aligned and length >= 16). Also given to modules in ml_api. ---------- */
ml_read:
    [--sp] = {rets, r14-r4}
    r3 = 0x01e4ff22
    call r3
    {pc, r14-r4} = [sp++]

/* ---- unsigned ml_crc(buffer, length): CRC-16/XMODEM in RAM ----------- */
ml_crc:
    [--sp] = {rets, r5-r4}
    r2 = 0
mlcrc_byte:
    if (r1 == 0) goto mlcrc_done
    r3 = b[r0+0] (u)
    r0 += 1
    r1 += -1
    r3 = r3 << 8
    r2 ^= r3
    r4 = 8
mlcrc_bit:
    r2 = r2 << 1
    r3 = r2 >> 16
    if (r3 == 0) goto mlcrc_next_bit
    r3 = 0x11021
    r2 ^= r3
mlcrc_next_bit:
    r4 += -1
    if (r4 != 0) goto mlcrc_bit
    goto mlcrc_byte
mlcrc_done:
    r0 = r2
    {pc, r5-r4} = [sp++]

/* ---- what a module gets in r0 ---------------------------------------- */
    .balign 4
ml_api:
    .long 0x50413948        /* "H9AP" */
    .long 1                 /* API version */
    .long 0x00cc1000        /* bundle chip address */
    .long 0x00029f00        /* load address */
    .long 0x2000            /* slot bytes */
    .long ml_read           /* void read(dst, chip_address, length) */
