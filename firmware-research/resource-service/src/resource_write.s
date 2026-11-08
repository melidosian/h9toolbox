/* TD-H9 resource-flash write service additions (S2).
 *
 * Linked after the unchanged S1 read service. Two stock call sites point here:
 *   app+0x05D018  clone 'R' byte read  -> resource_s2_read
 *   app+0x05D122  clone 'W' callback   -> resource_s2_write
 *
 * The 'W' site originally ran r7 = [r4+12]; r1 = r5 + 4; r2 = 32; r0 = r6;
 * call r7. The patch keeps the first three and replaces r0 = r6; call r7 with
 * one call here, so resource_s2_write starts with r0 = r6.
 *
 * Clone 'W' 0x4000-0x4FFF (below 0x5000, so no session write permission bit)
 * carries service commands; every other address goes to the stock callback.
 *   W 0x4000         control: "H9WR", op, chip address (24-bit LE), CRC16 (LE)
 *                    op 0 disarm
 *                    op 1 erase the 4 KiB sector, only if its current
 *                         CRC-16/XMODEM equals the supplied CRC
 *                    op 2 begin programming at a 32-byte aligned address, up
 *                         to the end of that 4 KiB sector
 *   W 0x4020-0x4FFF  data: program 32 bytes at the write pointer, only if they
 *                    are erased, then read back and compare
 * Only voice, font, and resource (stock selectors 1, 2, 5) are writable: the
 * partitions stock never writes itself. Any failure disarms. The stock handler
 * always ACKs a 'W', so results are reported in status bytes 12-19 of an
 * 'R' 0x4000 + 32n status slot (version 2):
 *   12 result  13 state (2 = programming)  14-16 write pointer
 *   17 completed operations (mod 256)  18-19 last sector CRC
 * Results: 0 ok, 1 bad magic, 2 flash not ready, 3 partition refused,
 * 4 misaligned, 5 sector CRC mismatch, 6 erase verify failed, 7 not armed,
 * 8 bad W checksum, 9 out of range, 10 target not erased, 11 program verify
 * failed, 12 bad op, 0xFE no command since boot.
 *
 * State lives in the same linker padding as the S1 bank word:
 *   0x1E4E8 write pointer   0x1E4EC write end (0 = not armed)
 *   0x1E4F0 result, state, count, 0   0x1E4F4 last CRC
 *   0x1E4F8 tag = sum of the four words + 0x48395752 (RAM is not cleared)
 */
    /* Placed in the second erased cave (app+0xA9D9E, 1,004 bytes) by
     * resource_service_s2.ld; the S1 code fills the first. */
    .section .s2,"ax",@progbits
    .globl resource_s2_read
resource_s2_read:
    [--sp] = {rets, r7-r4}
    r4 = r1
    r5 = r2
    r6 = resource_service_read
    call r6
    r7 = r0
    r1 = 0x4000
    if (r4 < r1) goto s2r_done
    r1 = 0x8000
    if (r4 >= r1) goto s2r_done
    r6 = r4 & 31
    if (r6 != 4) goto s2r_fields
    r0 = 2
    b[r5+0] = r0
    goto s2r_done
s2r_fields:
    if (r6 < 12) goto s2r_done
    if (r6 >= 20) goto s2r_done
    call s2_load
    r0 = r6 - 12
    r2 = s2_status_offsets
    r2 = r2 + r0
    r0 = b[r2+0] (u)
    r1 = 0x0001e4e8
    r1 = r1 + r0
    r0 = b[r1+0] (u)
    b[r5+0] = r0
s2r_done:
    r0 = r7
    {pc, r7-r4} = [sp++]

    .globl resource_s2_write
resource_s2_write:
    r0 = r6
    [--sp] = {rets, r7-r4}
    r4 = 0x4000
    if (r0 < r4) goto s2w_pass
    r4 = 0x5000
    if (r0 >= r4) goto s2w_pass
    call s2_service_write
    {pc, r7-r4} = [sp++]
s2w_pass:
    call r7
    {pc, r7-r4} = [sp++]

/* r0 = clone address 0x4000-0x4FFF, r1 = 32 data bytes followed by the sum. */
s2_service_write:
    [--sp] = {rets, r11-r4}
    sp += -32
    r4 = r0
    r5 = r1
    call s2_load
    r8 = 0x0001e4e8

    /* Copy the payload to the stack and check the W checksum byte. */
    r6 = 0
    r7 = 0
s2w_copy:
    r1 = r5 + r6
    r0 = b[r1+0] (u)
    r7 = r7 + r0
    r1 = sp + 0
    r1 = r1 + r6
    b[r1+0] = r0
    r6 += 1
    if (r6 != 32) goto s2w_copy
    r0 = b[r5+32] (u)
    r7 = r7 & 255
    r9 = 8
    if (r0 != r7) goto s2w_fail
    r9 = 2
    call s2_ready
    if (r0 == 0) goto s2w_fail
    r0 = 0x4000
    if (r4 != r0) goto s2w_data

    /* Control block. */
    r9 = 1
    r0 = [sp+0]
    r1 = 0x52573948
    if (r0 != r1) goto s2w_fail
    r1 = sp + 0
    r6 = b[r1+4] (u)
    r10 = b[r1+5] (u)
    r0 = b[r1+6] (u)
    r0 = r0 << 8
    r10 |= r0
    r0 = b[r1+7] (u)
    r0 = r0 << 16
    r10 |= r0
    r11 = b[r1+8] (u)
    r0 = b[r1+9] (u)
    r0 = r0 << 8
    r11 |= r0
    if (r6 == 0) goto s2w_disarm
    r9 = 3
    r0 = r10
    call s2_allowed
    if (r0 == 0) goto s2w_fail
    if (r6 == 1) goto s2w_erase
    if (r6 == 2) goto s2w_begin
    r9 = 12
    goto s2w_fail

s2w_disarm:
    r0 = 0
    [r8+4] = r0
    b[r8+9] = r0
    goto s2w_ok

s2w_erase:
    r9 = 4
    r0 = r10 << 20
    if (r0 != 0) goto s2w_fail
    r0 = r10
    call s2_crc_sector
    [r8+12] = r0
    r9 = 5
    if (r0 != r11) goto s2w_fail
    r0 = r10 >> 12
    r3 = 0x01e50000
    call r3
    r0 = r10
    call s2_crc_sector
    [r8+12] = r0
    r9 = 6
    r1 = 0xe03e
    if (r0 != r1) goto s2w_fail
    r0 = 0
    [r8+4] = r0
    b[r8+9] = r0
    goto s2w_ok

s2w_begin:
    r9 = 4
    r0 = r10 & 31
    if (r0 != 0) goto s2w_fail
    [r8+0] = r10
    r0 = r10 >> 12
    r0 += 1
    r0 = r0 << 12
    [r8+4] = r0
    r0 = 2
    b[r8+9] = r0
    goto s2w_ok

    /* Data block: program 32 bytes at the write pointer. */
s2w_data:
    r9 = 7
    r0 = b[r8+9] (u)
    if (r0 != 2) goto s2w_fail
    r10 = [r8+0]
    r0 = [r8+4]
    r1 = r10 + 32
    r9 = 9
    if (r0 < r1) goto s2w_fail
    r9 = 10
    r0 = r10
    r1 = 0
    call s2_compare32
    if (r0 == 0) goto s2w_fail
    r6 = 0
s2w_program:
    r0 = sp + 0
    r0 = r0 + r6
    r1 = r10 + r6
    r2 = 8
    r3 = 0x01e5006e
    call r3
    r6 += 8
    if (r6 != 32) goto s2w_program
    r9 = 11
    r0 = r10
    r1 = sp + 0
    call s2_compare32
    if (r0 == 0) goto s2w_fail
    r10 += 32
    [r8+0] = r10
    r0 = [r8+4]
    if (r10 != r0) goto s2w_ok
    r0 = 0
    [r8+4] = r0
    b[r8+9] = r0
    goto s2w_ok

s2w_fail:
    r0 = 0
    [r8+4] = r0
    b[r8+9] = r0
    b[r8+8] = r9
    goto s2w_save
s2w_ok:
    r0 = 0
    b[r8+8] = r0
    r0 = b[r8+10] (u)
    r0 += 1
    b[r8+10] = r0
s2w_save:
    call s2_save
    sp += 32
    {pc, r11-r4} = [sp++]

/* Validate the state block; reinitialize it if the tag does not match. */
s2_load:
    [--sp] = {rets, r7-r4}
    r4 = 0x0001e4e8
    call s2_tag
    r1 = [r4+16]
    if (r0 == r1) goto s2_load_done
    r0 = 0
    [r4+0] = r0
    [r4+4] = r0
    [r4+12] = r0
    r0 = 0xfe
    [r4+8] = r0
    call s2_save
s2_load_done:
    {pc, r7-r4} = [sp++]

s2_save:
    [--sp] = {rets, r7-r4}
    call s2_tag
    r1 = 0x0001e4e8
    [r1+16] = r0
    {pc, r7-r4} = [sp++]

s2_tag:
    r1 = 0x0001e4e8
    r0 = [r1+0]
    r2 = [r1+4]
    r0 = r0 + r2
    r2 = [r1+8]
    r0 = r0 + r2
    r2 = [r1+12]
    r0 = r0 + r2
    r2 = 0x48395752
    r0 = r0 + r2
    rts

/* 1 when stock my_app has marked the SPI NOR ready (0x5AA5). */
s2_ready:
    r1 = 0x00012234
    r1 = h[r1+0] (u)
    r2 = 0x5aa5
    r0 = 0
    if (r1 != r2) goto s2_ready_done
    r0 = 1
s2_ready_done:
    rts

/* 1 when chip address r0 lies in a writable partition (voice, font, resource). */
s2_allowed:
    r0 = r0 >> 20
    r1 = s2_allowed_mib
    r1 = r1 + r0
    r0 = b[r1+0] (u)
    rts

/* CRC-16/XMODEM of the 4 KiB sector at chip address r0, read 8 bytes at a
 * time so the stock reader stays on its byte path. */
s2_crc_sector:
    [--sp] = {rets, r9-r4}
    sp += -8
    r4 = r0
    r5 = 0
    r6 = 0
s2c_chunk:
    r0 = sp + 0
    r1 = r4 + r5
    r2 = 8
    r3 = 0x01e4ff22
    call r3
    r7 = 0
s2c_byte:
    r0 = sp + 0
    r0 = r0 + r7
    r0 = b[r0+0] (u)
    r0 = r0 << 8
    r6 ^= r0
    r8 = 8
s2c_bit:
    r6 = r6 << 1
    r0 = r6 >> 16
    if (r0 == 0) goto s2c_next_bit
    r0 = 0x11021
    r6 ^= r0
s2c_next_bit:
    r8 += -1
    if (r8 != 0) goto s2c_bit
    r7 += 1
    if (r7 != 8) goto s2c_byte
    r5 += 8
    r0 = 4096
    if (r5 != r0) goto s2c_chunk
    r0 = r6
    sp += 8
    {pc, r9-r4} = [sp++]

/* 1 when the 32 chip bytes at r0 equal buffer r1, or are all 0xFF when r1 is 0. */
s2_compare32:
    [--sp] = {rets, r9-r4}
    sp += -8
    r4 = r0
    r5 = r1
    r6 = 0
s2k_chunk:
    r0 = sp + 0
    r1 = r4 + r6
    r2 = 8
    r3 = 0x01e4ff22
    call r3
    r7 = 0
s2k_byte:
    r0 = sp + 0
    r0 = r0 + r7
    r0 = b[r0+0] (u)
    r1 = 0xff
    if (r5 == 0) goto s2k_compare
    r1 = r5 + r6
    r1 = r1 + r7
    r1 = b[r1+0] (u)
s2k_compare:
    if (r0 != r1) goto s2k_differ
    r7 += 1
    if (r7 != 8) goto s2k_byte
    r6 += 8
    if (r6 != 32) goto s2k_chunk
    r0 = 1
    goto s2k_done
s2k_differ:
    r0 = 0
s2k_done:
    sp += 8
    {pc, r9-r4} = [sp++]

s2_status_offsets:
    .byte 8,9,0,1,2,10,12,13
s2_allowed_mib:
    .byte 0,1,1,1,1,1,1,1,0,0,1,1,1,0,0,0
