# TD-H9 16 MiB resource flash: full map (2026-11-09)

From one full-chip fast read of a TD-H9 running stock v1.0.38-derived
firmware (16,777,216 bytes, SHA-256 `b6d60e2e…`, 2,584 s, 40,330 bulk
requests at 64 ms). The `resource` range matches the earlier two-pass backup
exactly except the 10 boot-logo sectors rewritten since, so the read is
consistent. The dump itself stays private: `sys_data` and `rw_freq` hold the
owner's settings and channels.

| Partition | Range | Used | Contents |
|---|---|---|---|
| filesystem | `0x000000–0x0FFFFF` | whole volume | **FAT12** (`MSDOS5.0`, 512-byte sectors, 1 sector/cluster, 2,048 sectors, 1 FAT of 7 sectors, 512 root entries, data at `0x5000`). Folders `1`–`4` hold 16 MP3 prompts: `0`–`9.MP3`, `BT`, `BT_CONN`, `BT_DCONN`, `CON_FAIL`, `CON_OK`, `FM` (~170 KB; MPEG-2 Layer III, 16 kHz, 32 kbps, mono; FAT dates 2018-09-20). **843 KiB of clusters free.** |
| voice | `0x100000–0x3FFFFF` | 2,716 KiB (679/768 sectors, ends `0x3A7000`) | JLFS container `tone` (header at `0x100000`, size `0x2A6690`, flags `0x83`), **81 files**, all header CRCs valid: `index.idx`, English `zero`–`nine.wav`, Chinese `cn_0`…`cn_10`, `cn_bai`, call reminders, `d_sound2.mp3`, `rx_sound.mp3`, … Entry data is at `0x100000 + offset`. The `.wav` files are RIFF/WAVE PCM, 11,025 Hz, 16-bit, 2 channels. |
| font | `0x400000–0x7FFFFF` | 3,164 KiB (791/1024, ends `0x717000`) | Magic `DD CC BB AA`, version `0x100`, then seven section offsets (`0x2A908`, `0x2AA08`, `0x8C460`, `0xB6E68`, `0xBB080`, `0x171EE8`, `0x1A4D20`): probably glyph tables per size or character set. Format not yet decoded. |
| sys_data | `0x800000–0x8FFFFF` | 28 KiB (7 sectors) | settings, starts `"HW102"` (private) |
| rw_freq | `0x900000–0x9FFFFF` | 16 KiB (4 sectors) | the Bluetooth programmer's clone image at `0x900000–0x90313F` (**confirmed**: equals a programmer `.img` byte for byte), plus one block at `0x905000` (private) |
| resource | `0xA00000–0xCFFFFF` | 124 KiB (31 sectors) | 22 RGB565 images, see `main-resource-flash.md` |
| record | `0xD00000–0xFFFFFF` | empty | (stock writes here) |

## Free space

- `resource`: 737 sectors, about 2.9 MiB (writable by the S2 service)
- `font`: about 932 KiB after `0x717000` (writable)
- `voice`: about 356 KiB after `0x3A7000` (writable)
- FAT volume in `filesystem`: 843 KiB of free clusters (the service refuses
  this partition)
- `record`: 3 MiB, empty, but stock owns it
