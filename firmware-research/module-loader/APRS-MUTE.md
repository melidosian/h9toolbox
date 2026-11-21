# Normal-mode APRS mute prototype

The resident dev bench build exposes APRS MUTE in the Toolbox APRS BEACON
screen. Menu toggles the selection and saves it before updating the live
speaker gate. The normal radio restores it from the same settings journal
at boot. The ABI 3 prototype built with `--aprs-mute` alone uses a fixed
speaker gate. The published installer leaves the speaker routing in stock form.

The patch intercepts stock speaker-opening calls at app+`0x7B7A0` (squelch)
and app+`0x706A2` (volume). When stock APRS is enabled, its RX side is B, and
the current receive frequency matches B but not A, it skips the speaker-on
request. All other calls, including speaker-off requests, tail-call the stock
routine with the original arguments. Equal A/B frequencies favor voice audio.
The mute gate adds no modem, RF register, settings, or resource flash writes.

The instructions are newly authored; the decision tree was established by
static comparison with the exact user-supplied M11 app. M11's temporary state
at RAM `0x2BE80` overlaps Toolbox's module space, so its state and persistence
code cannot safely be copied. The resident bench build reserves its own
startup-cleared RAM and reads its settings after the stock resource chip
initializer reports ready.

## Persistence evidence

The M11 app (SHA-256 `b2075a713d072076081ea33b3e32b0e25c9a302d01369c9227c1aaea91352136`)
reads two setting bytes at app+`0xA78AA` with `0x01E52CB2(4, 0x80000, dst, 2)`.
App+`0xA7864` and `0xA7A3A` write one byte each with `0x01E50128`, selector 4,
and offsets `0x80000` and `0x80001`. The resource addresses are `0x980000` and
`0x980001` in stock `rw_freq`. The four-byte RAM structure at `0x2BE80` adds
two runtime markers around those setting bytes.

Toolbox has a power-cut-tested two-sector journal in
`modules/common/toolbox_settings_pair.c`. Both candidate sectors at `0x3FC000`
and `0x3FD000` were verified erased on November 18. The installed GAME
directory at `0xA1F000` describes a separate allocation. The settings screen
checks that relationship before it writes and verifies each committed record.
The experimental loader blocks uploader writes to the two candidates and
the adjacent VFO and GPS sectors. The Bluetooth output gate still needs a
verified audio trace.

## Offline checks

`python test_aprs_mute.py` assembles the actual gate and executes those decoded
instructions in a bounded host model over APRS enable values, side values,
current/A/B frequencies and open/close requests. It verifies passthrough
arguments, balanced stack and A priority. This is not CPU or RF
hardware emulation. The package builder checks original hook bytes, the
existing pinned base, cave limits and exact changed flash sectors.

## Hardware acceptance still required

With a recoverable backup and an explicitly authorized bench flash:

1. Confirm stock boot, PTT+2 and hold-8 recovery and Toolbox module return.
2. APRS disabled: A and B voice, squelch close/open and volume remain normal.
3. APRS enabled on B: confirm packets still decode while speaker is silent,
   including volume changes and normal menus (not just the APRS app).
4. Dual-watch: alternate A voice and B APRS traffic, testing both transitions;
   confirm A voice remains audible and B does not inherit an open A speaker.
5. Equal A/B frequencies favor voice; non-B APRS routes remain unchanged.
6. Exercise TX completion and reacquisition, power cycling, and simultaneous
   background beacon use. Other speaker-opening paths may need further hooks.

Failure of any audio/decoding test means the prototype is not ready to ship.

`python3 -I test_modules.py --native` runs the host C tests with the local C
compiler and UBSan. A normal firmware build creates the satellite fixture
used by the satellite tracker test, which currently has a stock-base
fixture mismatch unrelated to APRS.
