/* TD-H9 resource-flash read service (S1, read-only).
 *
 * The stock BLE clone 'R' handler reads each reply byte with
 * 0x01E52CB2(4, clone_offset, byte_pointer, 1), the bounds-checked partition
 * reader for the 16 MiB SPI NOR. One call at app+0x05D018 is redirected here.
 *
 *   0x0000-0x3BFF  stock clone image read, unchanged
 *   0x3E00 / 0x3E20 / 0x3E40  slot 16 requests a 15-30 ms, latency-0 BLE
 *                  link; slot 17 restores stock's link parameters; slot 18
 *                  requests a fixed 15 ms, latency-0 link
 *   0x3C00-0x3D9F  bulk read slot k = (offset - 0x3C00) / 32: k 0-4 select
 *                  16, 48, 112, 176 or 240 data bytes per notification, k 8-12
 *                  the same sizes with bounded waiting for TX room. On the
 *                  reply's first byte, streams up to 2 KiB from the bulk
 *                  cursor as notifications on the programming handle (12),
 *                  each tagged B7 5A seq len, while the stack's TX queue keeps
 *                  room for the stock reply. The 32-byte reply reports it.
 *   0x4000-0x7FFF  32-byte status slot n = (offset - 0x4000) / 32; reading it
 *                  selects 32 KiB bank n (512 banks cover 16 MiB)
 *   0x8000-0xFFFF  chip byte at bank * 0x8000 + (offset - 0x8000)
 *
 * Chip bytes still go through the stock reader, using the partition that
 * covers the address; the seven stock partitions tile 0x000000-0xFFFFFF.
 * Nothing here erases or writes the resource flash.
 *
 * The bank word and its tag live at RAM 0x1E4E0/0x1E4E4, inside linker
 * padding between the stock .bss end variable (0x1E4D4) and the first heap
 * block (0x1E500). That RAM is not cleared at boot, so the tag is checked.
 * Bulk state uses the rest of that padding: 0x1E4D8 start | status << 24,
 * 0x1E4DC count | first TX capacity << 12 | room-after-wait polls << 22,
 * 0x1E4FC cursor | 0xB7 << 24 (tag).
 * Bulk reply bytes: 0-3 "H9BK", 4-11 the two result words, 12-15 the cursor
 * word, 16-31 zero. Status: 0 ok, 2 flash not ready or read failed, 3 TX
 * queue full before the first packet, 4 bad slot, 5 no bank selected,
 * 6 notification submit failed.
 *
 * COMPACT (defined only for the merged Brick Breaker + S2 build, where the
 * service shares the QR-margin caves with the game): drops slot 16 and the
 * in-radio wait (k 8-12 then behave like k 0-4, reporting 0 polls, which
 * pages already treat as "switch to no-wait"), and puts the code in .svc and
 * the byte tables in .tables so the linker can place them in separate caves.
 * Every other reply byte is unchanged.
 */
    .ifdef COMPACT
    .section .svc,"ax",@progbits
    .else
    .section .text,"ax",@progbits
    .endif
    .globl resource_service_read
resource_service_read:
    [--sp] = {rets, r7-r4}
    r4 = r1
    r5 = r2
    r6 = 0x4000
    if (r4 >= r6) goto service
    r6 = 0x3c00
    if (r4 >= r6) goto bulk_slot
    r6 = 0x01e52cb2
    call r6
    {pc, r7-r4} = [sp++]

service:
    r6 = 0x8000
    if (r4 >= r6) goto chip_byte

    /* Status slot: select the bank, then return one status byte. */
    r6 = 0x4000
    r6 = r4 - r6
    r7 = r6 >> 5
    r0 = 0x0001e4e0
    [r0+0] = r7
    r2 = 0x48390000
    r1 = r7 + r2
    [r0+4] = r1
    r1 = r7 << 15
    r2 = 0xb7000000
    r1 = r1 | r2
    r0 = 0x0001e4fc
    [r0+0] = r1
    r6 = r6 & 31
    r0 = 0
    if (r6 >= 12) goto store_byte
    if (r6 >= 8) goto status_tail
    if (r6 >= 5) goto status_jedec
    r1 = status_header
    r1 = r1 + r6
    r0 = b[r1+0] (u)
    goto store_byte

status_jedec:
    r0 = r6 - 5
    call jedec_byte
    goto store_byte

status_tail:
    if (r6 != 8) goto status_bank
    call flash_ready
    goto store_byte
status_bank:
    r0 = r7 & 255
    if (r6 == 9) goto store_byte
    r0 = r7 >> 8
    if (r6 == 10) goto store_byte
    r0 = 0x5a

store_byte:
    b[r5+0] = r0
    r0 = 0
    {pc, r7-r4} = [sp++]

chip_byte:
    r0 = 0x0001e4e0
    r6 = [r0+0]
    r1 = [r0+4]
    r2 = 0x48390000
    r3 = r6 + r2
    if (r1 != r3) goto chip_invalid
    r0 = 512
    if (r6 >= r0) goto chip_invalid
    r6 = r6 << 15
    r0 = 0x8000
    r1 = r4 - r0
    r6 = r6 + r1
    r0 = 0x01000000
    if (r6 >= r0) goto chip_invalid

    r0 = r6
    r1 = r5
    r2 = 1
    call chip_read
    {pc, r7-r4} = [sp++]

/* Read r2 (1-8) chip bytes at r0 into r1 through the stock partition reader,
 * using the partition that covers r0. Returns the reader's result. */
chip_read:
    [--sp] = {rets, r7-r4}
    r4 = r0
    r5 = r1
    r6 = r2
    r0 = r4 >> 20
    r1 = partition_of_mib
    r1 = r1 + r0
    r7 = b[r1+0] (u)
    r1 = r7 * 20
    r2 = 0x01eb53f4
    r1 = r1 + r2
    r1 = [r1+0]
    r1 = r4 - r1
    r0 = r7
    r2 = r5
    r3 = r6
    r4 = 0x01e52cb2
    call r4
    {pc, r7-r4} = [sp++]

/* Bulk slot: byte 0 runs the transfer, every byte returns the result block. */
bulk_slot:
    r6 = 0x3c00
    r6 = r4 - r6
    r7 = r6 >> 5
    r6 = r6 & 31
    if (r6 != 0) goto bulk_reply
    .ifndef COMPACT
    if (r7 == 16) goto bulk_link_fast
    .endif
    if (r7 == 17) goto bulk_link_stock
    if (r7 == 18) goto bulk_link_fixed
    r0 = r7
    call bulk_send
    goto bulk_reply
    .ifndef COMPACT
bulk_link_fast:
    r0 = link_params
    call link_request
    goto bulk_reply
    .endif
bulk_link_fixed:
    r0 = link_params_fixed
    call link_request
    goto bulk_reply
bulk_link_stock:
    r0 = 0x01e50316
    call r0
    r0 = 0x0001e4d8
    r1 = 0
    [r0+0] = r1
    [r0+4] = r1
bulk_reply:
    r0 = 0
    if (r6 >= 16) goto bulk_store
    if (r6 >= 12) goto bulk_cursor_byte
    if (r6 >= 4) goto bulk_result_byte
    r1 = bulk_magic
    r1 = r1 + r6
    r0 = b[r1+0] (u)
    goto bulk_store
bulk_result_byte:
    r1 = 0x0001e4d4
    r1 = r1 + r6
    r0 = b[r1+0] (u)
    goto bulk_store
bulk_cursor_byte:
    r1 = 0x0001e4f0
    r1 = r1 + r6
    r0 = b[r1+0] (u)
bulk_store:
    b[r5+0] = r0
    r0 = 0
    {pc, r7-r4} = [sp++]

/* Stream from the cursor as tagged notifications on handle 12.
 * Slot k 0-4: packet size 16/48/112/176/240, stop when the TX queue is full.
 * Slot k 8-12: same sizes, but poll the queue up to 100 times (20,000-loop
 * delay each) for room before stopping. Each packet carries
 * min(size, room - 68) data bytes in multiples of 8, so 64 bytes stay free:
 * the stock 37-byte reply plus the stack's per-notification overhead. A
 * 40-byte reserve left the reply without room on hardware (2026-11-08), and
 * stock then drops the reply silently. Packet: B7 5A seq len, then len data bytes.
 * Result word 0x1E4DC: count (bits 0-11), first TX capacity (12-21, capped at
 * 1023), polls that ended with room (22-31). */
bulk_send:
    [--sp] = {rets, r11-r4}
    sp += -248
    r6 = 0
    r7 = 0
    r11 = 0
    r9 = 4
    .ifndef COMPACT
    r4 = 0
    .endif
    if (r0 < 8) goto bulk_slot_size
    r0 += -8
    .ifndef COMPACT
    r4 = 1
    .endif
bulk_slot_size:
    if (r0 >= 5) goto bulk_result
    r1 = bulk_sizes
    r1 = r1 + r0
    r5 = b[r1+0] (u)
    r9 = 2
    call flash_ready
    if (r0 == 0) goto bulk_result
    r9 = 5
    r0 = 0x0001e4fc
    r6 = [r0+0]
    r1 = r6 >> 24
    if (r1 != 0xb7) goto bulk_no_cursor
    r6 = r6 << 8
    r6 = r6 >> 8
    r3 = 0x01e50286
    call r3
    r1 = 1023
    if (r0 < r1) goto bulk_cap_first
    r0 = r1
bulk_cap_first:
    r11 = r0 << 12
    r8 = 0
    r9 = 0
bulk_packet:
    r0 = 2048
    if (r7 >= r0) goto bulk_done
    r0 = r6 + r7
    r1 = 0x01000000
    if (r0 >= r1) goto bulk_done
    .ifdef COMPACT
    r3 = 0x01e50286
    call r3
    if (r0 >= 76) goto bulk_size
    .else
    r10 = 0
bulk_room:
    r3 = 0x01e50286
    call r3
    if (r0 >= 76) goto bulk_have_room
    if (r4 == 0) goto bulk_no_room
    r1 = 100
    if (r10 >= r1) goto bulk_no_room
    r10 += 1
    r0 = 20000
bulk_wait:
    r0 += -1
    if (r0 != 0) goto bulk_wait
    goto bulk_room
bulk_no_room:
    .endif
    if (r7 != 0) goto bulk_done
    r9 = 3
    goto bulk_done
    .ifndef COMPACT
bulk_have_room:
    if (r10 == 0) goto bulk_size
    r1 = r11 >> 22
    r2 = 1023
    if (r1 >= r2) goto bulk_size
    r1 = 0x00400000
    r11 = r11 + r1
    .endif
bulk_size:
    r0 += -68
    r0 = r0 >> 3
    r0 = r0 << 3
    if (r0 < r5) goto bulk_size_room
    r0 = r5
bulk_size_room:
    r1 = 2048
    r1 = r1 - r7
    if (r0 < r1) goto bulk_size_left
    r0 = r1
bulk_size_left:
    [sp+244] = r0
    r1 = sp + 0
    r2 = 0xb7
    b[r1+0] = r2
    r2 = 0x5a
    b[r1+1] = r2
    b[r1+2] = r8
    b[r1+3] = r0
    r10 = 0
bulk_read:
    r0 = r6 + r7
    r0 = r0 + r10
    r1 = sp + 4
    r1 = r1 + r10
    r2 = 8
    call chip_read
    if (r0 == 0) goto bulk_read_ok
    r9 = 2
    goto bulk_done
bulk_read_ok:
    r10 += 8
    r0 = [sp+244]
    if (r10 != r0) goto bulk_read
    r0 = 12
    r1 = sp + 0
    r2 = r10 + 4
    r3 = 0x01e5d2e0
    call r3
    if (r0 == 0) goto bulk_sent
    r9 = 6
    goto bulk_done
bulk_sent:
    r7 = r7 + r10
    r8 += 1
    r8 = r8 & 255
    goto bulk_packet
bulk_done:
    r0 = r6 + r7
    r1 = 0xb7000000
    r0 = r0 | r1
    r1 = 0x0001e4fc
    [r1+0] = r0
    goto bulk_result
bulk_no_cursor:
    r6 = 0
bulk_result:
    r0 = r9 << 24
    r0 = r0 | r6
    r1 = 0x0001e4d8
    [r1+0] = r0
    r0 = r11 | r7
    [r1+4] = r0
    sp += 248
    {pc, r11-r4} = [sp++]

/* Slot 16: ask the central for a 15-30 ms connection interval with slave
 * latency 0 (stock asks for latency 10, so an idle radio may skip up to ten
 * connection events before it sees a request). Same call as stock
 * 0x01E5033C: ble_user_cmd_prepare(11 = BLE_CMD_REQ_CONN_PARAM_UPDATE, 2,
 * connection handle h[0x11AD0+138], &params). Result: status 0 submitted,
 * 7 not connected, 8 stack returned an error (its code in the count field).
 * Slot 17 calls stock 0x01E50316, which re-requests stock's own parameters. */
link_request:
    [--sp] = {rets, r7-r4}
    r7 = r0
    r4 = 7
    r5 = 0
    r1 = 0x00011ad0
    r2 = h[r1+138] (u)
    if (r2 == 0) goto link_store
    r0 = 11
    r1 = 2
    r3 = r7
    r6 = 0x01e16652
    call r6
    r5 = r0 << 20
    r5 = r5 >> 20
    r4 = 0
    if (r0 == 0) goto link_store
    r4 = 8
link_store:
    r0 = r4 << 24
    r1 = 0x0001e4d8
    [r1+0] = r0
    [r1+4] = r5
    {pc, r7-r4} = [sp++]

chip_invalid:
    r0 = 0xff
    b[r5+0] = r0
    r0 = -1
    {pc, r7-r4} = [sp++]

/* Return 1 when stock my_app has initialized the SPI NOR (marker 0x5AA5). */
flash_ready:
    r1 = 0x00012234
    r1 = h[r1+0] (u)
    r2 = 0x5aa5
    r0 = 0
    if (r1 != r2) goto flash_ready_done
    r0 = 1
flash_ready_done:
    rts

/* Return JEDEC ID byte r0 (0 manufacturer, 1 type, 2 capacity), or 0 if the
 * flash is not initialized. Mirrors stock app+0x0884E4..0x088518: ROM lock,
 * chip select GPIO 23 low, 9F and three dummy bytes over SPI 2, select high,
 * ROM unlock. */
jedec_byte:
    [--sp] = {rets, r7-r4}
    r6 = r0
    call flash_ready
    if (r0 == 0) goto jedec_done
    r0 = 23
    r3 = 0x01e001ba
    call r3
    r4 = r0
    r7 = 0x001e0200
    r3 = 0x02001634
    call r3
    if (r4 == 0) goto jedec_selected
    r0 = [r7+0]
    [r4+0] &= 0xffffff7f
    r0 = [r7+0]
jedec_selected:
    r0 = 0x9f
    r3 = 0x01e4ff06
    call r3
    r0 = 0xff
    r3 = 0x01e4ff06
    call r3
    r5 = r0
    r0 = 0xff
    r3 = 0x01e4ff06
    call r3
    r5 = r5 << 8
    r5 = r5 | r0
    r0 = 0xff
    r3 = 0x01e4ff06
    call r3
    r5 = r5 << 8
    r5 = r5 | r0
    if (r4 == 0) goto jedec_released
    r0 = [r7+0]
    [r4+0] |= 0x80
    r0 = [r7+0]
jedec_released:
    r3 = 0x02001650
    call r3
    r0 = r5 >> 16
    if (r6 == 0) goto jedec_mask
    r0 = r5 >> 8
    if (r6 == 1) goto jedec_mask
    r0 = r5
jedec_mask:
    r0 = r0 & 255
jedec_done:
    {pc, r7-r4} = [sp++]

    .ifdef COMPACT
    .section .tables,"ax",@progbits
    .endif
status_header:
    .byte 0x48,0x39,0x52,0x53,0x01,0,0,0
partition_of_mib:
    .byte 0,1,1,1,2,2,2,2,3,4,5,5,5,6,6,6
bulk_magic:
    .byte 0x48,0x39,0x42,0x4b
bulk_sizes:
    .byte 16,48,112,176,240
    .balign 8
    .ifndef COMPACT
/* min 12 (15 ms), max 24 (30 ms), latency 0, timeout 600 (6 s) */
link_params:
    .short 12,24,0,600
    .endif
/* slot 18: min = max = 12 (15 ms), latency 0, timeout 600 (6 s). macOS chose
 * 30 ms from the 15-30 ms request on 2026-11-08. */
link_params_fixed:
    .short 12,12,0,600
