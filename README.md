# H9 Toolbox

Firmware reverse-engineering notes, offline analysis, the Windows/Linux USB-C flasher, and the HID recovery tools are collected in [firmware-research](firmware-research/README.md). They are separate from the Bluetooth web programmer below.

A small, buildless Web Bluetooth programmer for the TIDRADIO TD-H9. Open the HTTPS site in Chrome or Edge on a computer with Bluetooth. No account or cloud storage is used by the page itself; the radio image stays in browser memory until you download a backup.

**AI disclosure:** H9 Toolbox was entirely and responsibly AI generated under human direction. It is experimental software; saving an untouched backup and verifying settings before writing are recommended.

## GitHub Pages

**Open the programmer:** https://h9toolbox.com/ (HTTPS certificate provisioning)

The checked-in workflow publishes `dist/` directly to GitHub Pages on each push to `main`, or when **Deploy GitHub Pages** is run manually. GitHub Pages serves the site over HTTPS. Web Bluetooth still requires a supported browser; hosting on Pages does not add iPhone Safari support.

## Current workflow

1. Enable the TD-H9's Bluetooth programming mode.
2. Connect and read the radio.
3. Optionally save the untouched `.img` backup so you can recover the original configuration. You can also download readable JSON for transfer or editing.
4. Confirm the displayed channels and radio settings against the radio.
5. Edit channels or mapped radio settings, acknowledge the experimental software risk, and write the changed blocks. The app checks the radio's identity and original bytes before writing, waits for ACK, and reads each changed block back.

The channel editor covers RX/TX frequency, name, offset direction, FM/AM and wide/narrow mode, separate TX/RX CTCSS or DCS tones with polarity, transmit power, scan inclusion, channel enablement, PTT ID, busy lockout, hopping RX, and scramble code. It has direct Create and Remove buttons for channels 1–199. Changing RX also updates TX for simplex and common US repeater pairs. It suggests −600 kHz for 145.20–145.50 and 146.61–146.97 MHz outputs, +600 kHz for 147.00–147.39 MHz outputs, and −1.6 MHz for 223.85–224.98 MHz outputs. For 70 cm, the user chooses + or −5 MHz because direction varies locally. Existing custom splits follow RX changes, while a manually entered TX remains untouched until Use suggested TX is selected. ARRL's [band plan](https://www.arrl.org/band-plan) and [offset guidance](https://www.arrl.org/phone-patch-guidelines) are the source for these US defaults; individual repeater listings take precedence.

The radio settings editor maps basic radio behavior, VFO/scan, display/buttons, Bluetooth, SMS/Morse, DTMF controls, GNSS, APRS identity/beacon/relay/routing. Unknown values and unrelated bytes are preserved. Fixed GNSS coordinates are displayed but not edited. Stored DTMF codes, FM broadcast presets, and undocumented firmware bytes are not edited. If another H9 uses a nonstandard service, enter its service UUID in Bluetooth options and reconnect.

## Channel CSV and portable JSON

`Export channels CSV` saves populated channels and every editable channel option in a spreadsheet-friendly file, without radio settings or raw memory blocks. `Import channels CSV` applies it to a loaded image. It merges by channel number unless **Replace all channels** is selected; that option also clears occupied slots omitted from the CSV. To clear one channel, set its `action` column to `clear`. The app lists counts and asks before replacing or removing occupied channels. Older channels JSON exports are still accepted on import. Import only changes the working image until Write changes is used. Saving the original `.img` is recommended but is not required to write.

The CSV has columns for radio mode, channel number, action, name, RX/TX frequency, modulation, TX/RX tones and DCS polarity, power, scan and enabled flags, offset direction, PTT ID, scramble, busy lock, and hopping. Keep `radio_mode` equal to the loaded image mode. Boolean columns accept `true`/`false`, `yes`/`no`, `on`/`off`, or `1`/`0`. Tone types are `off`, `ctcss`, `dcs`, or `raw`; raw tones can only be preserved on their original channel.
Channel names starting with spreadsheet formula characters get a leading apostrophe in the CSV; the importer removes it on round trip.

`Download config JSON` exports a pretty-printed text file with populated channel objects and named settings. It has no raw memory blocks. To import it on another computer, first read the destination radio or open its `.img` backup, then use `Open JSON`. The importer merges listed channels and settings into that image; omitted channels remain unchanged. An explicit `{ "n": 1, "clear": true }` entry clears channel 1. Settings use display labels for option values and numbers for numeric fields. The destination image supplies any unknown bytes.

`Download archive JSON` is available under Lossless JSON archive. It includes the complete original image as 394 hex memory blocks and imports without a separate `.img`. Earlier version-1 JSON archives remain supported. `Download working .img` exports the currently edited image for other software. Both JSON variants contain callsigns and relay passwords; the archive also contains all other image bytes. Consider that before sharing them.

The radio's newer image after Morse was enabled has `0x302A = 0x25`, which maps to Morse on, 12 WPM and 700 Hz. An earlier `0xFF` control byte is treated as Off; switching it On initializes it to the observed `0x25`. `0x302B` remains `0xFF`, so key type, power, and RX threshold stay uninitialized and cannot be edited by this tool.

On 2026-11-03, a TD-H9 connected through Web Bluetooth and two complete Normal-mode reads produced byte-identical 12,616-byte backups. Channel 1 decoded as `2M-CALL`, 146.520 MHz RX/TX and matched the radio's display. The user reported that a live channel write looked successful. Extended setting writes have not yet been checked on the radio.

## Protocol notes

- Image: 8-byte TD-H9 mode identity plus 0x3140 memory bytes.
- Existing channel 1: radio offset 0x0010, 16 bytes per channel; RX and TX frequencies occupy 4 little-endian BCD bytes each, in 10 Hz units.
- Names: radio offset 0x0D40, 8 bytes per channel.
- Clone read: 32-byte blocks. The tested earlier firmware returned 36-byte BLE replies without a checksum. Firmware 1.0.38 can append a 37th byte; the reader now detects either length and retains manual options for unusual firmware.
- The tested TD-H9 advertised `FF00`; its `FF02` characteristic permits writes and `FF01` sends notifications. The browser profile uses this pair. Other firmware or hardware should still be checked on connection.
- Write: `W` + 16-bit address + length + payload + sum-of-payload checksum, followed by an ACK. Only changed blocks in the published writable ranges are sent.

References: [CHIRP TD-H9 issue and test module](https://chirpmyradio.com/issues/12216), [CHIRP BLE radio notes](https://chirpmyradio.com/projects/chirp/wiki/BLERadios), and [Web Bluetooth API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Bluetooth_API). Protocol implementation was written independently; no CHIRP code is included.
