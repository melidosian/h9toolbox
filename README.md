# H9 Toolbox

Free browser tools and custom firmware for the TIDRADIO TD-H9.

[Open H9 Toolbox](https://h9toolbox.com/) · [Explore the firmware](https://h9toolbox.com/firmware/) · [Bluetooth programmer](https://h9toolbox.com/programmer/) · [App Manager](https://h9toolbox.com/apps/) · [Changelog](CHANGELOG.md)

## H9 Toolbox Firmware

H9 Toolbox Firmware builds on the stock Type-C v1.0.38 firmware. It keeps
your saved channels and settings and adds an app menu. On v0.1.7, press
**Menu** from the channel screen to open Toolbox or hold **Back** for Quick
Change. On v0.1.5, hold **Back** for the app menu.

- **APRS.** View heard stations, distance and bearing, a map around your GPS
  position, and messages. Use the keypad to send a status packet or canned
  message. APRS transmission requires an amateur radio licence.
- **FM Satellite Passes.** See the next five passes for your location, plot
  them across the sky, and tune for Doppler shift during a pass.
- **GPS Info.** Check coordinates, altitude, your Maidenhead grid square,
  satellite signals, and UTC or local time.
- **SmartBeacon and APRS Mute.** Send automatic APRS position reports with
  SmartBeacon or on a timer, and mute incoming packet audio.
- **Full KISS TNC.** The optional APRS modem update enables packet
  connections over Bluetooth. The updater links to the bridge for KISS TCP
  and AGWPE clients such as Pat and Paracon. [Bridge setup](firmware-research/kiss-tnc/host/README.md)
- **Apps and boot logo.** Use App Manager to add or remove apps, check
  available space, or change the startup image over Bluetooth or experimental
  USB-C. The app catalog includes Brick Breaker and Maze Chase. [App Manager](https://h9toolbox.com/apps/)

### Install and recover

1. Open the firmware updater in Chrome or Edge on a computer. Connect a
   data-capable USB-C cable and hold **PTT + 2** while powering on the
   Type-C TD-H9. [Firmware updater](https://h9toolbox.com/firmware-update/)
2. Choose verified H9 Toolbox or stock firmware, review the flash map, then
   confirm the install. **Full** includes every available build option.
   **Essentials** includes the core Toolbox features. **Custom** lets you
   choose the Quick Change controls and APRS modem updater. Firmware v0.1.8
   adds experimental 33 cm controls and a **Standard** preset with Quick
   Change and the modem updater. Keep USB connected through verification.
   Select the optional `.h9hid` backup for an exact firmware rollback. The
   updater requests a reboot after verification and gives manual restart
   instructions if the radio stays in update mode.
3. With the radio on its normal channel screen, use the App Manager linked
   from your updater to choose apps and select **Write to radio**.
   Add or update **About**, then open **Setup → About** on the radio to see
   which build options are included in its firmware.
   [App Manager](https://h9toolbox.com/apps/)
4. If you want full KISS TNC and the updater verifies that the modem
   update was staged, follow **Optional: Unlock KISS TNC** on the restart
   step. It includes the keypad instructions and bridge download.

Optional backups use pinned Type-C stock references for v1.0.23, v1.0.24,
v1.0.27, v1.0.28, v1.0.29, v1.0.32, v1.0.33 and v1.0.38. The updater
checks the radio before reusing any reference bytes. For factory recovery,
the installer offers Type-C v1.0.38.

Keep any `.h9hid` file you create for an exact firmware rollback. The Bluetooth
programmer’s `.img` backup is for channels and settings. To return to the
factory main firmware, choose **Stock Type-C v1.0.38** in the updater. The
APRS modem uses its own recovery procedure. [TD-H9 recovery guide](https://dev.h9toolbox.com/firmware-update/recovery/) · [APRS modem recovery](firmware-research/kiss-tnc/README.md#aprs-modem)

## Project files

- [Module loader and apps](firmware-research/module-loader/README.md) contains the Toolbox sources and build instructions.
- [Firmware research](firmware-research/README.md) contains the analysis, replacement-source work, USB flasher, and HID recovery tools.
- [VFO control discovery](prototypes/vfo-control/README.md) contains the Bluetooth remote-control experiment and hardware observations.
- [Doom for the TD-H9](https://h9toolbox.com/doom/) uses separate experimental firmware to run Freedoom’s first level.
- The active site lives in `dist/`. Keep radio backups, probe captures, and official firmware packages in the Git-ignored `private/downloads/` directory.

The `htcommander` repository is a separate project with its own TD-H9 integration.

## Hosting

GitHub Pages builds `main` with `scripts/build-site.sh` for
[h9toolbox.com](https://h9toolbox.com/). Cloudflare Pages builds `dev` with
`scripts/build-dev-site.sh` for [dev.h9toolbox.com](https://dev.h9toolbox.com/)
and records that commit at [`/dev-build.txt`](https://dev.h9toolbox.com/dev-build.txt).
The production installer defaults to H9 Toolbox Firmware v0.1.7.
The `dev` deployment stages changes before promotion to `main` and currently
serves v0.1.8 with experimental 33 cm support. Both deployments serve the main
firmware, far-flash payload and matching App Manager catalog.

The production site uses Google Analytics 4 for page views and automatic
interaction events. The tag runs only on `h9toolbox.com`. H9 Toolbox keeps
radio images, channels, and settings out of Analytics.

**AI disclosure:** H9 Toolbox was entirely and responsibly AI generated under human direction.

## Bluetooth programmer

Open the HTTPS programmer in Chrome or Edge on a computer with Bluetooth.
The radio image stays in browser memory until you download a backup. TD-H9
owners have reported successful programming on several radios. Save an
untouched backup and review your channels and settings before writing.
[Open the Bluetooth programmer](https://h9toolbox.com/programmer/)

### Programming workflow

1. Enable the TD-H9's Bluetooth programming mode.
2. Connect and read the radio.
3. Optionally save the untouched `.img` backup so you can recover the original configuration. You can also download readable JSON for transfer or editing.
4. Confirm the displayed channels and radio settings against the radio.
5. Edit channels or mapped radio settings, confirm you have reviewed your changes, and write the changed blocks. The app checks the radio's identity and original bytes before writing, waits for ACK, and reads each changed block back.

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

Since that initial check, users have reported successful Bluetooth programming on several radios. These reports support the everyday programming workflow; they do not establish validation of every mapped setting or the separate browser firmware updater.

## Protocol notes

- Image: 8-byte TD-H9 mode identity plus 0x3140 memory bytes.
- Existing channel 1: radio offset 0x0010, 16 bytes per channel; RX and TX frequencies occupy 4 little-endian BCD bytes each, in 10 Hz units.
- Names: radio offset 0x0D40, 8 bytes per channel.
- Clone read: 32-byte blocks. The tested earlier firmware returned 36-byte BLE replies without a checksum. Firmware 1.0.38 can append a 37th byte; the reader now detects either length and retains manual options for unusual firmware.
- The tested TD-H9 advertised `FF00`; its `FF02` characteristic permits writes and `FF01` sends notifications. The browser profile uses this pair. Other firmware or hardware should still be checked on connection.
- Write: `W` + 16-bit address + length + payload + sum-of-payload checksum, followed by an ACK. Only changed blocks in the published writable ranges are sent.

References: [CHIRP TD-H9 issue and test module](https://chirpmyradio.com/issues/12216), [CHIRP BLE radio notes](https://chirpmyradio.com/projects/chirp/wiki/BLERadios), and [Web Bluetooth API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Bluetooth_API). Protocol implementation was written independently; no CHIRP code is included.
