# TD-H9 APRS modem receive fix

The separate APRS 1.0.15 modem converted a received `0xFF` byte to `-1`
after removing it from the queue. The receive parser treated that return
as an empty queue. The frame then failed its FCS check because it was
missing the byte. This image preserves the original unsigned return and
retains the RX15 change for 15-byte AX.25 control frames.

## Windows update

Keep an original TIDRADIO `APRS-1.0.15.bin` available for recovery. Enter
**APRS Update** with PTT+7 at power-on and connect the H9 via USB-C. From
the root of this checkout, run the command below at 38,400 baud. Substitute
the radio's COM port and the path to the stock file.

```powershell
py -3 firmware-research/h9-firmware-research/aprs_flasher.py flash `
  firmware-research/kiss-tnc/APRS-1.0.15-rx15-rxff.bin `
  --port COM4 --baud 38400 --allow-modified `
  --release-manifest firmware-research/kiss-tnc/APRS-1.0.15-rx15-rxff.release.json `
  --recovery-image "C:\path\to\stock\APRS-1.0.15.bin" --yes
```

The updater configures DTR and RTS before opening the port and waits for
20 ms of serial silence before writing the first block. After the transfer
is acknowledged, power-cycle normally and check that APRS Info shows
`V1.0.15`. If the modem stays on `Querying…`, use the same updater with
the stock image and omit the modified-image options. A stock restore on
Windows has previously been verified by the radio's version query.

## Validation

The builder pins the stock hash and checks the two original instructions.
Relative to stock, the image changes three bytes across two instructions.
Replaying the captured HDLC bits through the original RX15 binary
reproduced the drop of `I[0/4]` before UART output. The patched binary
forwarded the full frame in emulation, preserved all 256 byte values, still
passed 15-byte control frames, and rejected a corrupt FCS. The Mac transfer
received all block and exit acknowledgments, but the radio continued to
show `Querying…` after reboot. The patched image has no verified hardware
boot yet.

```powershell
py -3 firmware-research/kiss-tnc/build_aprs_rx.py `
  "C:\path\to\stock\APRS-1.0.15.bin" APRS-1.0.15-rx15-rxff.bin
```
