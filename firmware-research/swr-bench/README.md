# TD-H9 SWR bench firmware

This experimental Type-C TD-H9 build uses stock v1.0.38 and records one
transmit event per power cycle. It captures raw ADC channels 0, 3, 4, 10,
and 13, the stock TX flag, the ADC scan cursor, and timestamps. The existing
timer callback still runs. The recorder samples its RAM cache at up to
25 Hz and never keys the radio. [SWR research notes](../resource-flash-log.md#2026-11-23--td-h9-transmit-swr-feasibility)

The output is an ADC trace. It takes a calibrated RF detector and a
reference measurement to turn a repeatable response into SWR. Boot,
held-PTT timing, ADC freshness, and Bluetooth export on the H9 still
need a physical-radio test.

## First bench run

1. Start with a rated 50 Ω dummy load. With the H9 disconnected, use the
   MFJ analyzer to measure the load and adapters at the intended VHF or
   UHF frequency. Record the frequency, SWR and, if available, resistance
   and reactance. Use the band supported by your MFJ model.
2. Disconnect the MFJ completely, then attach the load to the H9. The
   analyzer generates its own test signal. Feeding H9 transmit power
   into its test port can damage it.
3. Set the H9 to Low and use simplex on the measured frequency. Disable
   VOX, automatic APRS and PTT-linked beacons. After these settings are
   in place, power-cycle with the load attached, wait for the channel
   screen and a few seconds of idle, press PTT for about one second, then
   release. Wait another second before connecting via Bluetooth.
4. In Chrome or Edge on a computer with Bluetooth, open the local reader.
   Choose **Read via Bluetooth** and **Download CSV** after it finds the
   frozen trace. Keep power on until the trace is saved. A reboot clears
   it and arms the next capture.
5. Repeat the matched-load run. For comparison, characterize a rated
   1.5:1 or 2:1 test load with the MFJ disconnected from the H9, then
   connect that load to the H9 and repeat under the same conditions.
   A matched load alone can verify capture and show battery droop.
   Keep the frequency, power setting and fixture fixed across the pair.

Serve the repository root, or the directory where the bench archive was
unpacked, with a local server.

```sh
python3 -m http.server 8000 --bind 127.0.0.1
```

Open `http://localhost:8000/firmware-research/swr-bench/`. Use normal
on-radio Bluetooth programming mode when the reader asks to connect.
Close other apps connected to the H9 before exporting.

## Read the trace

Use the first TX sample as time zero. Look for advancing timestamps
and repeated changes of the ADC scan cursor. The recorder can run even
if stock stops updating its ADC cache during transmit. A quiet ADC
channel with a moving cursor is a different observation from a quiet
channel with a frozen cursor. Independently check that the stock red TX
status and the captured TX flag agree.

ADC 4 is the battery control and ADC 10 and 13 are keypad controls.
During early bench work, compare ADC 0 and 3 across the matched and
known-mismatch loads and check whether either also follows knob or key
movement. A sample that simply changes on PTT establishes a TX-linked
change. To identify a detector, confirm its circuit and repeat the
response across known loads, bands, supply voltage, and reflection
phases. The MFJ characterizes the load at analyzer power. Measuring
what the H9 actually delivers under transmit still requires a
directional reference meter.

## Build, install and recover

Build from the repository root with the pinned stock inputs and the
bundled JieLi toolchain. [Stock inputs and toolchain](../setup/README.md)

```sh
python3 -I firmware-research/swr-bench/build.py \
  --stock-image /path/to/app.bin \
  --stock-package /path/to/TD-H9-V1.0.38.fw \
  --toolchain /path/to/jieli-linux-toolchains-20260730.1 \
  --output /path/to/new-bench-build
```

The packaged file is `h9-swr-bench-OFFLINE.fw`. It temporarily runs the
stock main application with the recorder, so Toolbox's on-radio app menu
is unavailable during the test. The package uses the normal app flash
range and leaves the settings, resource flash, far Toolbox payload and
APRS modem outside its write range.

Use the existing BR23 flasher's custom-package path with the pinned
stock package as `--stock-base`. Inspect the package offline first. In
USB update mode entered by holding **8** at power-on, use a new backup
filename for this test and retain the backup, its manifest, and both
packages. The existing flasher restores the saved pre-test main image.
**PTT + 2** provides the independent HID stock recovery path.
[Custom package install and exact backup restore](../h9-flasher/README.md#custom-main-image-offline-validation-and-recovery)
· [PTT + 2 stock recovery](../hid-recovery/README.md)

## Implementation

- The boot hook at app+`0x088734` clears the recorder and registers its
  wrapper with the original timer arguments. The wrapper calls stock
  `0x01E896C8` and reads only the cache, TX byte and clock. The normal
  radio superloop continues to own PTT and the display.
- The build reserves 2 KiB at the end of the stock heap. The recorder
  uses `0x2B700` and stops after 12 samples past release or 100 total
  sample slots. Up to 16 slots hold pretrigger samples.
- The Bluetooth clone read hook serves the versioned 1,632-byte log at
  `0x3C00..0x425F`. The reader downloads the capture over the existing
  clone link. Sample data becomes available after the trace is frozen.

Native capture tests and Node reader tests run without radio hardware.

```sh
cc -std=c11 -Wall -Wextra -Werror -O2 \
  firmware-research/swr-bench/capture.c firmware-research/swr-bench/test_capture.c \
  -o /tmp/h9-swr-test
/tmp/h9-swr-test
node --test firmware-research/swr-bench/test_read_capture.mjs
```
