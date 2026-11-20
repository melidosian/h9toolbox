/* Pinned v1.0.38 Bluetooth service boundaries. No stock menu drawing entry is
 * called. Preserve the JieLi ABI's complete callee-saved register set. */
    .section .text,"ax",@progbits
    .macro bt_call name, target
    .globl \name
\name:
    [--sp] = {rets, r14-r4}
    r4 = \target
    call r4
    {pc, r14-r4} = [sp++]
    .endm

    bt_call radio_bt_vm_read, 0x01e2c52a
    bt_call radio_bt_mic_native, 0x01e50804
    bt_call radio_bt_speaker_native, 0x01e50820
    bt_call radio_bt_alloc_native, 0x01e30f32
    bt_call radio_bt_role_native, 0x01e6a878
    bt_call radio_bt_discover_native, 0x01e4e79c
    bt_call radio_bt_saved_native, 0x01e6a992
    bt_call radio_bt_connect_native, 0x01e75820
    bt_call radio_bt_cleanup_native, 0x01e6a88a
    bt_call radio_bt_command_native, 0x01e17932
    bt_call radio_bt_reset_native, 0x01e71060
    bt_call radio_bt_partition_native, 0x01e52cb2
