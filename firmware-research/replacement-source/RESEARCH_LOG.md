# TD-H9 replacement firmware research log

This is the chronological record of display experiments, source ports, and
offline link checks. Some entries describe their state when written; use the
[current status](README.md) for the latest verified hardware result.

## Stage-twenty-three Snake coordinate correction

The stage-twenty-two hardware trial booted and displayed green snake cells and
red food, but both appeared as stretched horizontal stripes with a green line
across the screen. The supplied videos showed the stripe moving and the board
restarting. In `snake_draw_cell`, division placed the row's pixel coordinate in
`r0` and the column's in `r1`; the call to `hello_set_window` needs x then y.
The end coordinates were already x then y. `main/display_task_entry_stage23.s`
swaps the two start registers before opening each cell window. The stage-22
stock backup was restored through HID, and all 213 accessible main-firmware
sector CRCs matched stock afterward.

The corrected offline package under ignored `build/display-task-stage23/` has
SHA-256 `dac982915910fc49076d1eac8cc01581ca78f5eacb92b44096c2b710805515d0`;
target flash SHA-256 is
`0b62151294309a2e6ce6c406970b9894b6ba14a4cdb996baf9ed9cac6f6d67f9`.
It changes the same four sectors. The fresh stock backup, four-sector write,
and full flash readback passed on hardware. The user reported that the display
and Snake controls looked and worked great after the fix. Food growth and
wall-collision behavior were not separately described. Stock was restored
through HID; all 213 accessible main-firmware sector CRCs matched stock, and
the user confirmed the normal channel screen and controls returned.

## Stage-twenty-two minimal Snake trial

`main/display_task_entry_stage22.s` turns the proven four-button display task
into a 10×8-cell Snake game. Each 16×16 cell uses the verified 128-byte RAM
transfer buffer. The three-cell green snake starts moving right toward a red
food cell. Up/Down/Menu/Back turn it up/down/left/right, with direct reverse
turns ignored. Reaching food grows the body and moves the food to another
unoccupied cell. It waits for a valid direction key before starting each
round; Back starts it moving right from the initial position. Wall or body
collision gives three LED pulses, clears the board, and returns to this
ready state. Input requires a release before another direction is
accepted, reducing the double-turn behavior seen once in stage twenty-one.
The game keeps a 32-cell maximum body in task stack RAM. It removes the
blocking startup LED patterns and uses the batched full-screen clear. Hardware
testing exposed the LCD window coordinate bug described in stage twenty-three.

The offline package under ignored `build/display-task-stage22/` has SHA-256
`f9c7aac8cfed340df934a5b3319bf95446e8791a7e0fcb1a1f2f8dcd12338858`;
target flash SHA-256 is
`211c5313a555e4724eff147e60ac08ad2cb00a3fbed060d9be83d2b71c8a091f`.
It changed the same four sectors as the display trial. A fresh-backup/flash
script and pinned HID restore path were used; the distorted display was
observed and stock was restored through HID.

## Stage-twenty-one faster four-direction marker trial

`main/display_task_entry_stage21.s` keeps the hardware-confirmed key decoder
and Up/Down movement from stage twenty. It adds candidate Menu/Back key codes
12 and 15 for left/right movement. The square remains inside x=8..136 and
y=8..104. Each move erases the old 16×16 area and draws the new one using
four 128-byte transfers apiece. Pixel bytes are copied from flash into a RAM
buffer before transfer, as in the verified stack-buffer clear path. The
blocking LED pulse after each move is removed; the four startup pulses remain.

The offline package under ignored `build/display-task-stage21/` has SHA-256
`958d0a052e402c4f43576a615764074032e883122227e8d356d6d58f8e994335`;
target flash SHA-256 is
`af02e095c740f0dd48046000ba1c30393918e466d0eb5f314d950c6ac8b48bc5`.
It changes the same four sectors. Deterministic rebuild, package inspection,
PowerShell parsing, and pinned HID restore plan pass offline. Hardware
redraw was markedly faster, and Up/Down/Menu/Back moved in the requested
directions. One Down press appeared to also move right once; the cause was
not confirmed. Stock was restored through HID, all 213 accessible main
firmware sector CRCs matched stock, and the normal UI returned.

## Stage-twenty stock key-decoder movement trial

`main/display_task_entry_stage20.s` retains the successful authored display
path. Stock `my_app` calls the key decoder at `0x01E515AC` in its loop; that
decoder reads the early pin registry and writes a current key code at RAM
`0xC3AF`. The new image calls it during each poll and moves the square eight
pixels for new codes 13 and 14. This isolates the failed stage-nineteen
event-byte assumption. Code-to-button mapping remains a hardware question.

The offline package in ignored `build/display-task-stage20/` has SHA-256
`afb132993929b31c53963e7634f3e953e0d0b96f80e653efba8ea22b04463f18`;
target flash SHA-256 is
`c72299e22f590e83e2551e724934886724674cad605f20124b7039c548149d3b`.
It changes the same four sectors. A reproducible rebuild, package inspection,
and pinned HID recovery plan pass. The stage-twenty trial script does backup
and flash in one Administrator run.

The stage-twenty trial made and hash-checked a fresh stock backup, verified
four written sectors, then lost its Administrator process during the final
full-image readback. A pinned `resume` read the full image successfully and
found it identical to the target, without another write. The display and key
response were confirmed: the red square moved Up and Down on the matching
buttons. Refresh felt slow. Stock was restored through HID; all 213
accessible main-firmware sector CRCs matched stock and the normal channel
screen and controls returned.

## Stage-nineteen Up/Down marker-movement trial

`main/display_task_entry_stage19.s` extends the hardware-confirmed
stage-seventeen display path. After drawing the 16×16 red square with its white
border, it polls the two adjacent key-state bytes at RAM `0xC3C2` and
`0xC3C3`. The pinned stock key table at app+`0xAF050` pairs those addresses
with key codes `13` and `14`; their physical Up/Down roles and whether these
bytes remain live while this replacement `my_app` task runs are hypotheses.
Each new nonzero state moves the square eight pixels, clamped to y=8..104.
The old square is erased and only the new 16×16 region is redrawn. The task
uses a short stock delay between polls so the stock scanner can run. Its LED
gives the familiar one/two/three/four startup milestones, then one pulse per
detected movement; it does not repeat four pulses while idle.

The offline package under ignored `build/display-task-stage19/` has SHA-256
`c14e3bf4193d9c902f81cf6c1d896a24177caf6eff84d193075efa1971657eff`;
target flash SHA-256 is
`5b1abcfbc16c4dd87e7082c4ff36c43c3b6d7070189970ed2e603ec59b3161a2`.
It changes only the same four sectors as the proven display trial. Rebuild,
PowerShell parsing, package inspection, and the pinned HID recovery plan pass
offline. `../h9-firmware-research/trial_display_task_stage19.ps1 -Action trial`
combines fresh backup and flash in one Administrator session. Hardware
observation: the marker booted at the usual 9–10 seconds and the startup
flashes stopped as designed, but Up/Down neither moved it nor caused a
movement flash. Stock was restored through HID and all 213 accessible
main-firmware sector CRCs matched stock. Normal screen and controls returned.

## Stage-eighteen batched full-screen clear

`main/display_task_entry_stage18.s` keeps the stage-seventeen authored panel
startup and replaces its 20,480 separate zero-color word writes with 320
transfers of a 128-byte zero block on the `my_app` task stack. The lower
transfer wrapper and ROM lock pair remain; stock code also accepts variable
transfer lengths and chunks at no more than 256 bytes. This should reduce
the roughly 9.6-second path to the final LED pattern without changing the
black background or red marker. The image still depends on stock reset,
`app_core`, controller setup, and lower transfer completion.

The offline package under ignored `build/display-task-stage18/` has SHA-256
`5770acd938d054fec31bd2dc4a1bafe4186bec08f7ac1d84cf7923219a640d6c`;
target flash SHA-256 is
`1e0d4e48d588d05cae82c63f8a51f55685db8516bd728493d1074a93394e7a6c`.
The same four sectors change. Rebuild, package inspection, and pinned HID
recovery-plan checks passed. The trial and fallback scripts are
`../h9-firmware-research/trial_display_task_stage18.ps1` and
`../h9-firmware-research/restore_display_task_stage18_over_hid.py`.
Hardware observation: black background, red marker, four LED flashes, and
about 9.5 seconds to display. The batched clear did not materially reduce
startup from stage seventeen's approximately 9.6 seconds. Stock was restored
through HID and the full accessible main-firmware CRC map matched stock.

## Stage-seventeen authored full-screen clear

`main/display_task_entry_stage17.s` replaces stage sixteen's stock fill
helper call with our window and 16-bit word transport wrappers. It selects the
full 160×128 LCD window and sends 20,480 zero-color pixels before backlight
setup and the red marker. This removes the stock fill helper dependency but
retains stock reset, `app_core`, controller setup, and lower transfer
completion. LED milestone three follows the clear, so a long delay before it
would locate a clear-loop problem. The package under ignored
`build/display-task-stage17/` has SHA-256
`91ceced49b41e9b152113be0380570428e47786d1f376ccd50c0c22d7edb8a8a`;
target flash SHA-256 is
`42e411864adb18d485838d4dd3066da347eedb42d150d999282d0e1b66493a1b`.
It changes the same four sectors. Rebuild, package inspection, and pinned HID
recovery-plan checks passed. The trial and fallback scripts are
`../h9-firmware-research/trial_display_task_stage17.ps1` and
`../h9-firmware-research/restore_display_task_stage17_over_hid.py`.
On 2026-11-06, a fresh stock-matching backup preceded the four-sector flash,
which passed full readback. The user measured about 9.6 seconds to the final
repeating four-flash LED pattern, confirming that the 20,480-word authored
clear loop and marker write completed. The user confirmed a black background
and red square with white border. PTT+2 HID inspection found exactly the four
changed sectors; fallback restored them and verified all 213 accessible main
sectors against stock. The user confirmed the normal channel screen and
controls after power cycling.

## Stage-sixteen cleared panel and color-order trial

`main/display_task_entry_stage16.s` adds the stock full-screen color-zero
fill helper call after the authored panel command sequence. It passes the
stock startup arguments `(0, 0, 160, 128, 0)` and then sets the corrected
backlight descriptor. `build_display_task_stage16.py` emits a 16×16 marker
with white border and big-endian RGB565 red center (`F8 00` bytes). This tests
whether the stage-fifteen noise was uncleared panel memory and its blue center
was byte order. The image still depends on stock reset, `app_core`, controller
setup, fill helper, and lower transfer completion.

The offline package under ignored `build/display-task-stage16/` has SHA-256
`0b15a693ddc0b8bdd403a7766f43615110229b0cbdad4c9a2aba9c9a77ab4548`;
target flash SHA-256 is
`8e8306ddeab5d73679df3481042f35812e48c88fd46f845b2d9653b5964e8071`.
The same four sectors change. Rebuild, package inspection, and pinned HID
recovery-plan checks passed. The trial and fallback scripts are
`../h9-firmware-research/trial_display_task_stage16.ps1` and
`../h9-firmware-research/restore_display_task_stage16_over_hid.py`.
On 2026-11-06, a fresh 892,928-byte backup matched pinned stock; the
four-sector flash passed full readback. On normal boot, the user confirmed a
solid black background and the intended red center square with white border.
This is the first clean display bring-up from the authored `my_app` task entry.
It still uses stock reset, `app_core`, controller setup, stock full-screen fill,
and lower transfer completion. PTT+2 HID inspection showed exactly four
changed sectors; fallback restored them and verified all 213 accessible main
sectors against stock. The user confirmed normal screen and controls after
the final power cycle.

## Stage-fifteen authored panel-startup retry

`main/display_task_entry_stage15.s` replaces the stock `my_app` task entry,
performs the authored panel reset and LCD command sequence from stage seven,
and draws a 16×16 red marker with white border. It now uses the stage-fourteen
verified descriptor halfword `0xFF14` at `+20` and computes the backlight duty
from RAM `0x12BAA`. LED milestones are one at entry, two after controller
setup, three after panel commands, and repeating four after pixel transfer;
the pulse timing is slower. Stock reset, `app_core`, controller setup, and
lower transfer completion remain dependencies.

The offline package under ignored `build/display-task-stage15/` has SHA-256
`221fa055db6f92d7406df89bd09ada54bd7ff7f42efe30c505b7d2d9b4c3dfde`;
target flash SHA-256 is
`c431c0b35bcfc1fd5aeea791bce7c116d351ea1f555b7b5dfb94ced9ef46016b`.
The same four sectors change. Rebuild, package inspection, and pinned HID
recovery-plan checks passed. The trial and fallback scripts are
`../h9-firmware-research/trial_display_task_stage15.ps1` and
`../h9-firmware-research/restore_display_task_stage15_over_hid.py`.
On 2026-11-06, a fresh stock-matching backup preceded the four-sector flash,
which passed full readback. The user reported a blue 16×16 center with white
border and multicolor noise in the untouched area; their photo confirmed both.
The center marker's clean geometry validates the authored panel reset,
command stream, backlight setup, and byte transport as a boot-visible path.
The surrounding noise is uninitialized LCD memory because this task entry
omitted the stock full-screen clear. The blue rather than intended red center
is consistent with little-endian RGB565 bytes sent to an MSB-first panel.
PTT+2 HID inspection found only four changed sectors; the fallback restored
them and verified all 213 accessible main sectors against stock. The user
confirmed normal screen and controls. Stage sixteen tests clear and byte order.

## Stage-fourteen corrected backlight descriptor

`main/hello_display_hook_stage14.s` keeps stock panel initialization and
recreates the backlight descriptor, with the corrected halfword `0xFF14` at
offset `+20`. The earlier `1872` assumption missed a Pi32v2 parallel-issue
dependency: `r1 = 1872  #` issues alongside `h[r0+20] = r1`, so that store sees
the previous `r1 = 0xFF14`; the new 1872 is then used to address the `+24`
byte relative to stock `r6`. Stage thirteen's five-flash result identified
this exact field mismatch. Stage fourteen also retains the slower LED timing.

The offline package under ignored `build/hello-display-stage14/` has SHA-256
`9d09ad59894bdc549a60926b1ffad92be2539c1898641f67661065fe80c8dc21`;
target flash SHA-256 is
`27520d9a0085970c7f2f65ee99ec2a8326aaf0d3660df27a9aa2caf790c07cf8`.
The same four sectors change. Rebuild, package inspection, and pinned HID
recovery-plan checks passed. The trial and fallback scripts are
`../h9-firmware-research/trial_hello_display_stage14.ps1` and
`../h9-firmware-research/restore_hello_display_stage14_over_hid.py`.
On 2026-11-06, a fresh backup matched pinned stock; the four-sector flash
passed full readback. The user saw `HELLO WORLD` with four slow repeating LED
flashes. This confirms that the corrected authored descriptor can drive the
backlight when stock panel initialization is retained. PTT+2 HID inspection
found exactly the four changed sectors; fallback restored them and verified
all 213 accessible main sectors against stock. The user confirmed normal
screen and controls. Stage fifteen applies the correction to the authored
panel-startup task.

## Stage-thirteen fixed-field locator

`main/hello_display_hook_stage13.s` reads the stock backlight descriptor and
draws `HELLO WORLD` without changing the descriptor. Slow final LED groups
identify the first fixed-field mismatch: one pulse for offset `+0`, two for
`+4`, three for `+8`, four for `+16`, five for `+20`, six for `+24`; seven means
all six match, and eight means the descriptor pointer differed. The package
under ignored `build/hello-display-stage13/` has SHA-256
`e21d31773648b9a3857ede445a1cf4d9e0241d3ec94435d3eaf48e9c0e7a9c30`;
target flash SHA-256 is
`fc6e488807de81794063d805b00a1751bdee5aaacc32a62f7f2f489747d5ea50`.
The same four sectors change. Rebuild, format inspection, and pinned HID
recovery-plan checks passed. The trial and fallback scripts are
`../h9-firmware-research/trial_hello_display_stage13.ps1` and
`../h9-firmware-research/restore_hello_display_stage13_over_hid.py`.
On 2026-11-06, a fresh stock-matching backup preceded the four-sector flash,
which passed full readback. The user observed five slow flashes repeating.
The stage-thirteen code maps this to descriptor offset `+20`. PTT+2 HID
inspection showed exactly four expected changed sectors; fallback restored
them and verified all 213 accessible sectors against stock. The user then
entered USB Update mode for the corrected stage-fourteen trial.

## Stage-twelve slow-duty-write diagnostic

`main/hello_display_hook_stage12.s` repeats stage eleven's read-only checks,
then writes the existing duty halfword back only if all fields match. Its LED
loops are eight times longer on/off and five times longer between groups, so
the final one/two/three/four count can be read reliably. `HELLO WORLD` should
remain visible if the duty rewrite is harmless. The offline package under
ignored `build/hello-display-stage12/` has SHA-256
`3eed20b08a7a600fd5ec8c41d14e065c0058919e6d983f794bf47d2134a2be87`;
target flash SHA-256 is
`c4de8d9e985c26c4a0cec759666a443ff234c37cb6cc270f4901116b684924d4`.
The same four sectors change. Rebuild, package inspection, and pinned HID
recovery-plan checks passed. The trial and fallback scripts are
`../h9-firmware-research/trial_hello_display_stage12.ps1` and
`../h9-firmware-research/restore_hello_display_stage12_over_hid.py`.
On 2026-11-06, a fresh stock-matching backup preceded the four-sector flash,
which passed full readback. The user saw `HELLO WORLD` and a clear slow pattern
of two flashes, pause, two flashes. The code therefore found a mismatch in one
of the six fixed descriptor fields and skipped the duty rewrite. The longer
LED delays explain the slower display appearance. HID restore found only the
four expected changed sectors and verified all 213 accessible sectors against
stock; the user confirmed normal screen and controls. Stage thirteen narrows
which fixed field differs.

## Stage-eleven read-only descriptor diagnostic

`main/hello_display_hook_stage11.s` reads, but never rewrites, the stock
backlight descriptor immediately before the original backlight call. It checks
the pointer, six fixed fields, and the duty field against the saved-brightness
formula. It then draws `HELLO WORLD` through the proven stage-six transport.
After drawing, repeating LED groups mean: one pulse for a pointer mismatch,
two for another field mismatch, three for a duty mismatch, four if every
checked value matches. The stock PWM helper saves r9, which holds this result.

The offline package under ignored `build/hello-display-stage11/` has SHA-256
`6ce671595b044aa3f3262ba2b1febb8b17d8e1ca507a93dd1d21ae4a60d60ef6`;
target flash SHA-256 is
`f837e0fc3e22ce402cee8c6619ddfd482c3cbfc016c39f8879af4fa9c7509d88`.
It changes the same four main sectors and passes reproducible build, package
inspection, and pinned HID recovery-plan checks. The trial and fallback
scripts are `../h9-firmware-research/trial_hello_display_stage11.ps1` and
`../h9-firmware-research/restore_hello_display_stage11_over_hid.py`.
On 2026-11-06, the first read-only BR23 backup stopped at 262,144 bytes;
its retry completed and matched stock. The first flash command also stopped
at 262,144 bytes during its initial read, before any write. A separate full
read confirmed stock was still present. The flash retry wrote four sectors and
passed full readback. The user saw `HELLO WORLD` and thought the fast final
LED group contained four flashes. PTT+2 HID inspection showed exactly four
changed sectors; the fallback restored them and verified all 213 accessible
main sectors. The user confirmed normal screen and controls. The inferred
four-pulse result needs a slower confirmation, implemented in stage twelve.

## Stage-ten code-placement control

`main/hello_display_hook_stage10.s` keeps the proven stage-six behavior and
stock backlight descriptor untouched. Twenty-four redundant two-byte moves
place its backlight call and every subsequent instruction at exactly the same
addresses and bytes as stage nine. This controls for a possible code-placement
effect: visible text would implicate stage nine's descriptor writes; a blank
screen would require investigating the common shifted code path. The offline
package under ignored `build/hello-display-stage10/` has SHA-256
`037a9cea140f0ba34ac02fe5734714628b84e3cf498b7017777e1af153fc5b8c`;
target flash SHA-256 is
`adca1c02b61d8b6f97c51093b9124cb40cf84c81e17317ff7efe2e287cf4dc08`.
Only the same four main sectors change; offline rebuild, format, and HID
recovery-plan checks passed. The trial and fallback scripts are
`../h9-firmware-research/trial_hello_display_stage10.ps1` and
`../h9-firmware-research/restore_hello_display_stage10_over_hid.py`.
The first stage-ten Administrator backup launch was canceled at Windows UAC;
no write occurred then. On retry, a fresh 892,928-byte backup matched pinned
stock SHA-256 `74243eaed19fe0c3e5fbb3ef3c46442f71ee8f7c216f214d322894d26ce4e648`.
The four-sector flash passed full readback, and the user saw `HELLO WORLD` on
normal boot. Thus the stage-nine code placement itself works, while its
descriptor-rewrite block correlates with the blank screen. PTT+2 HID inspection
found exactly the four expected sectors changed; the fallback restored them
and verified all 213 accessible main sectors against stock. The user confirmed
normal channel screen and controls after the final power cycle.

## Stage-nine saved-brightness trial

`main/hello_display_hook_stage9.s` retains stock panel initialization and
recreates the observed backlight descriptor, including the stock duty formula
`2000 + 2000 * RAM[0x12BAA]`. It then draws `HELLO WORLD` through the proven
stage-six transport. This isolates stage eight's forced minimum duty. The
offline package under ignored `build/hello-display-stage9/` has SHA-256
`9d0f6498c5b8c3fa80fe2fb643d16d42fa5bf842526347202f6a70b1f643223a`;
target flash SHA-256 is
`4f376e7d60fe04cd5c0306575c791035d31d0b51f89b1883b287e49b7d7709c3`.
Rebuild, format inspection, and pinned HID recovery-plan checks passed. Only
the same four main sectors change. The pinned trial and recovery scripts are
`../h9-firmware-research/trial_hello_display_stage9.ps1` and
`../h9-firmware-research/restore_hello_display_stage9_over_hid.py`.
On 2026-11-06, a fresh stock-matching backup preceded the four-sector flash;
full readback matched the target hash. The user reported a blank screen and
four LED flashes on normal boot. HID restoration verified the four restored
sectors and all 213 accessible main sectors against stock, and the user
confirmed normal channel screen and controls. The stock formula alone did not
restore visible output; stage ten tests whether the shifted code placement
is involved.

## Stage-eight backlight isolation trial

`main/hello_display_hook_stage8.s` hooks the proven post-panel-init stock
backlight call at app+`0x8841E`. It writes the seven observed backlight
descriptor fields itself, calls the stock backlight driver, then uses the
stage-six LCD transport to draw `HELLO WORLD`. It retains stock panel
initialization. If text appears, the stage-seven blank screen points toward
panel initialization or reset rather than this descriptor. If it stays blank,
the descriptor/backlight path needs more tracing. The LED repeats four pulses
after pixel writes in either case.

The offline package under ignored `build/hello-display-stage8/` has SHA-256
`a4fe90dbcaeca30c78f8ddc9750ed958753a21340f57c8f5ae5dd6b41f617dac`,
with target flash SHA-256
`6c4174a58d1bd04105865174e11e707d09875fbe3d1be3f371e038ce1887b84c`.
Rebuild and format checks passed; only sectors `0x5000`, `0x8D000`,
`0xAC000`, and `0xD9000` differ from stock. The pinned trial script is
`../h9-firmware-research/trial_hello_display_stage8.ps1`; the HID fallback is
`../h9-firmware-research/restore_hello_display_stage8_over_hid.py`.
On 2026-11-06, a fresh 892,928-byte backup matched pinned stock SHA-256
`74243eaed19fe0c3e5fbb3ef3c46442f71ee8f7c216f214d322894d26ce4e648`.
The image flashed through BR23 USB Update mode and full readback matched the
target hash. On normal boot, the user reported a blank display and likely
repeating four-pulse LED groups. HID inspection showed exactly the four
expected sectors changed; HID restore verified all 213 accessible main
sectors against stock. The user confirmed normal screen and controls after
power cycling.
The forced `2000` duty field at descriptor offset `+12` may be below this
radio's stock brightness setting: stock computes that field as
`2000 + 2000 * setting_byte`. Do not repeat this image. A follow-up should
preserve or measure the stock-computed field before changing panel startup.

## Stage-seven display task entry trial

`build_display_task_entry_stage7.py` replaces the stock `my_app` task entry at
app+`0x88180` with authored Pi32v2 assembly in the erased cave at app+`0xA77F0`.
The assembly repeats the observed early pin setup, panel reset, LCD command
sequence, backlight descriptor, window selection, and RGB565 byte stream. It
draws a 16×16 red marker with a white border. LED milestones are one pulse on
entry, two after controller setup, three after panel initialization, and
repeating four after pixel transfer. Stock reset, `app_core`, controller
setup, and low-level transfer completion remain dependencies; this trial does
not demonstrate an independent replacement reset path.

The pinned package is under ignored `build/display-task-stage7e/`. Its SHA-256
is `978526e95f820732e3d48cc15f680df84aebc73a96e4a8f3dc0b97451d47b134`;
the target main flash SHA-256 is
`d3bdf37aa32ff3d1bddf7a8907f6d490a25f16f546a36a8fbf85603593baef3f`.
Offline rebuild and format inspection passed; only sectors `0x5000`, `0x8D000`,
`0xAC000`, and `0xD9000` differ from stock. The fresh backup at
`private/downloads/h9-before-display-task-stage7-20261106.bin` matched stock
SHA-256 `74243eaed19fe0c3e5fbb3ef3c46442f71ee8f7c216f214d322894d26ce4e648`.
The pinned HID fallback is
`../h9-firmware-research/restore_display_task_stage7_over_hid.py`.
On 2026-11-06, the four-sector image flashed through BR23 USB Update mode and
full readback matched the target hash. On normal boot, the user reported a
blank, slightly dark screen and what appeared to be repeating four-pulse LED
groups. This suggests that code reached the pixel-loop exit, but does not
prove that the panel accepted the bytes or that the backlight was enabled.
PTT+2 HID mode showed exactly the four expected changed sectors; the pinned
fallback restored them and verified all 213 accessible main sectors against
the stock CRC map. The user confirmed the normal channel screen and controls
after power cycling. Do not repeat this image; isolate panel power/backlight and LCD
command acceptance in a more targeted experiment.

## HELLO WORLD display transport experiment

`build_hello_display_hook.py` creates an offline, stock-derived main package
that calls authored Pi32v2 assembly after the stock `my_app` task initializes
and clears the 160×128 LCD. The hook replaces the four-byte call at
app+`0x8841E`, then calls the original backlight setup, selects a 66×8 pixel
window, streams a generated RGB565 rendering of `HELLO WORLD` through the
stock LCD byte writer, and holds the task there. The code and pixels fit in
a 1,704-byte erased cave at app+`0xA77F0`; the successful stage-three image
uses 1,204 bytes. This keeps
the stock startup and LCD transport intact to verify our drawing path before
porting those drivers into the standalone boot probe.

```powershell
py -3 firmware-research/replacement-source/build_hello_display_hook.py --stock-image private/downloads/stock-main-extracted/app.bin --stock-package private/downloads/H9_Update-Type-C_41dc2974-af11-4ae0-8302-3fe4d775de0c/H9_Update-Type-C/TD-H9-V1.0.38.fw
py -3 firmware-research/h9-flasher/h9_flasher.py inspect firmware-research/replacement-source/build/hello-display-main/h9_hello_display_OFFLINE_EXPERIMENT.fw --stock-base private/downloads/H9_Update-Type-C_41dc2974-af11-4ae0-8302-3fe4d775de0c/H9_Update-Type-C/TD-H9-V1.0.38.fw
```

The built package SHA-256 is
`fcefde9a6958a13deabdb3ae9e79d8b8668ef75cd9dcbda76346f0a72a5c9a0e`.
Offline validation confirms that its 892,928-byte flash payload changes only
sectors `0x5000`, `0x8D000`, `0xAC000`, and `0xD9000`; its stock-derived app
repack passes the flasher's format check. The pinned HID fallback is
`../h9-firmware-research/restore_hello_display_over_hid.py`.

On 2026-11-06, a fresh radio backup matched the pinned stock image. The
four-sector package flashed with a full byte readback matching target SHA-256
`1b731db95725ff8870512074760f07bbe36dd0e42221534c940f0a3709dd5abe`.
On normal power-on, the user saw a blank screen; no `HELLO WORLD` text was
visible. PTT+2 HID mode showed exactly the four display-image sectors changed
and all 209 other accessible main sectors matching stock. The HID fallback
restored those four sectors, then verified all 213 accessible sectors by CRC.
The user confirmed the usual channel screen and working controls after normal
power-on. **Do not flash this package again.** The hook's execution stage is
not yet known. The stock-derived hook is a
transport experiment, not a standalone replacement startup. The pinned
`../h9-firmware-research/trial_hello_display.ps1` script prepares a new
read-only backup, checks the exact BR23 disk and package hashes for a trial,
and has a BR23 restore action. It has not been used on hardware. After a
successful display test, the LCD initialization, byte transport, and backlight
control still need to move into the authored reset path before the broader
replacement main firmware can claim the same result independently of stock
task startup.

`main/hello_display_hook_stage2.s` is a revised offline diagnostic. It keeps
the same hook and pixel data but drives the previously verified GPIO9 LED:
one pulse at hook entry, two after the original backlight call, three after
the LCD window call, and repeating groups of four after all pixel bytes are
sent. It also preserves `r0` across the original backlight call. Its package
under `build/hello-display-stage2/` has SHA-256
`8aa95ace8938881cf3bd4e8c049cf2bc0ad37e74792c967decd7a6cb0b764dae`;
the embedded flash SHA-256 is
`848e2ebae2bcc0a4b2c658894aa2793910b2f7d660a22c8f9a0e988bcaca94c5`.
The package and its pinned HID fallback passed offline checks. In a second
hardware trial, it flashed and passed full readback, but normal power-on gave
one LED pulse and a solid black screen. The HID restore of all four sectors
passed its 213-sector CRC scan, and the user confirmed normal screen and
controls afterward. Do not flash stage two again. Independent stock disassembly
then identified the fault: app+`0x079CC6` dereferences `r0` as a descriptor
pointer, while the diagnostic LED call had replaced that register with pulse
count 1 before calling the original function. The first image also replaced
`r0` with the function address. A third offline build in
`build/hello-display-stage3/` saves `r0` in `r8` at hook entry and restores it
before the original backlight call. Its package SHA-256 is
`606eea3a5bff8e0536665e6e28a89136568de2ef45c3ff0e6f5ed55ebe88420c`.
It passed offline package and HID recovery-plan validation before the trial.
Rebuild the successful revision with:

```powershell
py -3 firmware-research/replacement-source/build_hello_display_hook.py --source firmware-research/replacement-source/main/hello_display_hook_stage3.s --stock-image private/downloads/stock-main-extracted/app.bin --stock-package private/downloads/H9_Update-Type-C_41dc2974-af11-4ae0-8302-3fe4d775de0c/H9_Update-Type-C/TD-H9-V1.0.38.fw --output firmware-research/replacement-source/build/hello-display-stage3
```

The third trial completed on hardware on 2026-11-06. A fresh main-flash backup
matched stock SHA-256
`74243eaed19fe0c3e5fbb3ef3c46442f71ee8f7c216f214d322894d26ce4e648`.
The four-sector flash passed full-byte readback against image SHA-256
`f5566c573ad0be20623624ee33ec44f6330314eb021e338caef574eebf895cb4`.
After a normal power-on, the user saw **HELLO WORLD** on the display and
repeating groups of four top-LED pulses. The latter confirms the authored
pixel-stream loop completed. The test firmware intentionally holds the radio
task at that screen. The pinned PTT+2 HID fallback restored the four changed
sectors, verified all 213 accessible main-firmware sectors against stock by
CRC. The user confirmed the normal channel screen and working controls after
power-cycling. The experimental package remains under the ignored build
directory; the radio was returned to stock.

### Next function experiment: owned LCD data-byte wrapper

`main/hello_display_hook_stage4.s` replaces the stock data-byte wrapper at
`0x01E4ED6E` with authored assembly. The new wrapper places one pixel byte
on the stack, calls ROM `0x02001634`, passes `(1, byte_pointer, 1)` to the
stock lower transport at `0x01E4BEC4`, then calls ROM `0x02001650`. It keeps
the successful stage-three panel setup, window writer, pixel data, and LED
checkpoints. The ROM calls' internal roles are not yet established. A visible
`HELLO WORLD` and repeating four-pulse LED pattern would verify this wrapper
with the stock lower transport; it would not prove independent LCD startup or
hardware transport.

```powershell
py -3 firmware-research/replacement-source/build_hello_display_hook.py --source firmware-research/replacement-source/main/hello_display_hook_stage4.s --stock-image private/downloads/stock-main-extracted/app.bin --stock-package private/downloads/H9_Update-Type-C_41dc2974-af11-4ae0-8302-3fe4d775de0c/H9_Update-Type-C/TD-H9-V1.0.38.fw --output firmware-research/replacement-source/build/hello-display-stage4
```

The offline package SHA-256 is
`40be238d6727b70697f1c34508787133b46a68c93419a120a360e269417ef61a`;
its embedded flash SHA-256 is
`88d3a9625c854d23cb0b05915fc946371d345e7ba59b7af143acfeeadd70a08b`.
Inspection and a pinned HID recovery plan pass against the stock image.
`../h9-firmware-research/trial_hello_display_stage4.ps1` requires a fresh
stock backup before a hardware write. On 2026-11-06, that backup matched the
pinned stock image. The radio flashed four sectors and passed full readback
against the stage-four flash SHA-256 above. After normal power-on, the user
saw **HELLO WORLD** and repeating four-pulse LED groups. HID inspection then
found exactly the four stage-four sectors changed and all other accessible
main sectors matching stock. The fallback restored those sectors and verified
all 213 accessible sectors against stock by CRC. The user confirmed the
normal channel screen and working controls after disconnecting USB and
power-cycling.

### Stage five: owned LCD command and window path

`main/hello_display_hook_stage5.s` keeps the stage-four data-byte wrapper and
ports the stock window sequence (`0x2A` column range, `0x2B` row range,
`0x2C` pixel-memory write), the command/data GPIO pin-18 switch, and the
big-endian two-byte coordinate writer into authored assembly. Panel startup,
the GPIO pin-bank lookup, the lower transport, and its ROM calls remain stock.
The same LED milestones distinguish entry, backlight setup, window setup,
and completed pixel streaming.

```powershell
py -3 firmware-research/replacement-source/build_hello_display_hook.py --source firmware-research/replacement-source/main/hello_display_hook_stage5.s --stock-image private/downloads/stock-main-extracted/app.bin --stock-package private/downloads/H9_Update-Type-C_41dc2974-af11-4ae0-8302-3fe4d775de0c/H9_Update-Type-C/TD-H9-V1.0.38.fw --output firmware-research/replacement-source/build/hello-display-stage5
```

The reproducible offline package SHA-256 is
`cafcc37e938006ed3ec2c50320cf91a8c5129d57392197835e98002c6cc5e6c7`;
its embedded flash SHA-256 is
`edbe9e4541e5394a78c617e8f1f17879da8cb3a4a0b154bf9a03b0ebefb4715a`.
It changes the same four 4 KiB sectors as stage four. Package inspection,
source disassembly, and the pinned HID restore plan passed offline checks.
On 2026-11-06, a fresh radio backup matched the pinned stock image. The
four-sector flash passed full readback against the stage-five flash SHA-256
above. On normal power-on, the user saw **HELLO WORLD** and repeating
four-pulse LED groups, confirming that the authored command, window, word,
byte, and pixel-loop path completed. HID inspection found exactly those four
sectors changed and all other accessible main sectors matching stock. The
pinned fallback restored the four sectors and verified all 213 accessible
sectors against stock by CRC. The user confirmed the normal channel screen
and working controls after disconnecting USB and power-cycling. The radio is
back on stock firmware.

### Stage six: owned LCD transfer submission

`main/hello_display_hook_stage6.s` redirects both byte and coordinate-word
transfers to an authored copy of the stock app+`0x04BDC4` submission behavior.
It normalizes the controller index using the stock helper at `0x01E4BC8E`,
selects the controller register block, writes the buffer pointer and length,
issues `csync`, and calls the stock transfer completion function at
`0x01E4BD0E`. Panel and controller initialization remain stock. The same
`HELLO WORLD` image and LED checkpoints test whether this new boundary works.

```powershell
py -3 firmware-research/replacement-source/build_hello_display_hook.py --source firmware-research/replacement-source/main/hello_display_hook_stage6.s --stock-image private/downloads/stock-main-extracted/app.bin --stock-package private/downloads/H9_Update-Type-C_41dc2974-af11-4ae0-8302-3fe4d775de0c/H9_Update-Type-C/TD-H9-V1.0.38.fw --output firmware-research/replacement-source/build/hello-display-stage6
```

The offline package SHA-256 is
`1d6fca78735058df08ff7e3e9a3844c33b0eb04820bce606d8d01e000f119785`;
its embedded flash SHA-256 is
`e2bb2f954d01dbac8434a6476543d84be107ad2af31a462ce292ba02848642a7`.
It uses 1,432 of the 1,704 erased cave bytes and changes the same four 4 KiB
sectors. The package, rebuilt binary, disassembly, and pinned HID restore
plan passed offline checks. On 2026-11-06, a fresh radio backup matched the
pinned stock image and the four-sector flash passed full readback against the
stage-six flash SHA-256 above. After normal power-on, the user saw
**HELLO WORLD** and repeating four-pulse LED groups. This verifies the
authored transfer submission with the stock controller setup and completion
routine. HID inspection found exactly the four stage-six sectors changed;
the fallback restored them and verified all 213 accessible main sectors
against stock by CRC. The user confirmed the normal channel screen and
working controls after disconnecting USB and power-cycling. The radio is
back on stock firmware.

## Minimal main boot probe

`build_main_boot_probe.py` builds a separate 208-byte Pi32v2 program at the
stock `0x01E00100` app entry. It reuses the verified stock launch-argument copy
and stack setup, clears the stock `0xE2E0..0x1E4D4` RAM interval, writes
`0x48394254` and its complement to RAM words `0xE2E0` and `0xE2E4`, then
configures the stock breathing LED's GPIO 9 and blinks three pulses followed
by a pause. [The stock trace](../h9-firmware-research/evidence/stock/main-visible-boot-signal.md)
links the breathing LED setting and timer to GPIO bank 0 bit 9. The two RAM
words remain a debugger-visible sign that execution reached C after RAM
initialization. The probe has no display, scheduler, or radio drivers.

```powershell
py -3 firmware-research/replacement-source/build_main_boot_probe.py --compiler 'C:\path\to\pi32\bin\clang.exe' --stock-image 'C:\path\to\decrypted\app.bin'
```

The build verifies the launch ABI bytes, RAM clear bounds, entry address,
LED blink symbol, unresolved symbols, and stock-size raw layout. The raw
app preserves all pinned stock bytes after the 208-byte probe. Its `build/boot-probe-main/`
output contains an ELF, map, raw app layout, and manifest. The raw file is
for offline inspection only. A stock-derived `.fw` overlay passed
`h9_flasher.py inspect --stock-base` and differs from stock in 2 of 218
flash sectors (the first app sector and metadata near `0xD9000`). On
2026-11-06, the verified overlay was flashed over a fresh stock backup.
Both sectors and the complete `0xDA000`-byte readback verified. After a normal
power-on, the user observed the repeated three-pulse LED pattern. This proves
that the replacement entry executed far enough to drive GPIO 9. The RAM marker
was not read on hardware, and the probe has no display or radio functions.

For a failed boot, the main flasher's `restore` uses the fresh backup created
before the flash. A second, HID-specific fallback is
`../h9-firmware-research/restore_boot_probe_over_hid.py`. Its `plan` command
validates the exact probe package and stock main-flash backup without a device;
`inspect` checks the radio's accessible main-firmware CRC map in PTT+2 mode;
`restore --yes` rewrites only sectors `0xD9000` and `0x5000`, with the app
header last. After the boot test, the normal BR23 disk did not enumerate in
update mode. PTT+2 produced the solid-green-light `4C4A:4155` HID interface.
The read-only scan found exactly the two probe sectors changed and 211 other
accessible main-firmware sectors matching stock. The fallback restored the
two sectors, verified all 213 accessible sectors by CRC, and the user confirmed
the normal channel screen and working controls after power-cycling. The first
`0x5000` bootloader bytes were unchanged by the probe and are outside the HID
CRC map. The stock backup is
`private/downloads/h9-before-boot-probe-20261106.bin` (SHA-256
`74243eaed19fe0c3e5fbb3ef3c46442f71ee8f7c216f214d322894d26ce4e648`).

This is the beginning of a source rewrite for the **main Pi32v2 firmware** and the separate **ARM APRS controller**. The isolated main boot probe and stock-derived display hook above have booted; the broader standalone replacement source is not a complete radio firmware image. Source behavior below is tied to the captured stock v1.0.38 main app (`app.bin` SHA-256 `11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`) and APRS v1.0.15 image (SHA-256 `1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33`). Neither vendor image is checked into the repository.

## Implemented source

- A replacement-only [APRS burst activity command](aprs-burst-mute.md) now carries decoder activity from the APRS processor to the main processor. A 250 ms renewal and 750 ms main-side lease bound the effect of a lost clear. The mute decision requires the user option and a verified APRS receive route. Both audible output gates, UI setting, and live route source remain unconnected; this does not yet mute speaker or Bluetooth audio.

- `common/h9_aprs_wire.*` encodes and decodes the `0xC0`-delimited, `0xDB`-escaped serial framing observed at APRS addresses `0x08004894` and `0x08005B8C`. The controller now streams outgoing frames in 64-byte fragments instead of reserving a 2,043-byte frame buffer in RAM; its UART callback must consume or copy each fragment before returning. A maximum-length all-escaped frame matches the original contiguous encoder in host tests.
- `aprs/h9_aprs_controller.*` implements the observed command dispatch for `0x00`, `0x10`, `0x11`, `0x13`, and `0x20`, packet forwarding, and the ADC callback ordering at `0x08002EF8`: demodulate every sample before an optional TX waveform update. Board-specific UART, GPIO, ADC, DAC, modem, and packet TX operations are explicit callbacks.
- The APRS serial parser also accepts the stock raw one-byte TX settings (`0x01`, `0x04`, `0x03`, `0x02`, and `0x0F`) as soon as each data byte arrives. Lead and tail updates reach the diagnostic TX service; the third 10-ms setting is stored pending a verified runtime use. The stock defaults are 500 ms lead, 100 ms tail, 200 ms for the third setting, and persistence 63. See [the serial settings trace](../h9-firmware-research/evidence/stock/aprs-tx-serial-settings.md).
- `aprs/h9_aprs_status.*` reconstructs the 48-byte command `0x13` reply layout at `0x0800586C`: ten little-endian words, a few separately stored state bytes, and a 16-bit tick difference. A host test sends the record through the controller and parses the framed reply. `aprs/h9_aprs_runtime_status.*` fills that envelope from live replacement UART/APRS counters and its 1 ms tick. Words 4 and 5 now report failed and successful USART2 recovery operations. Stock counts individual DMA start attempts within a retry, so the values are replacement diagnostics and are not directly comparable. See [the status trace](../h9-firmware-research/evidence/stock/aprs-status.md).
- `aprs/h9_aprs_systick.*` now provides a real vector-15 timer source in the diagnostic APRS board entry. The replacement configures the stock 1 ms tick step, decoded from the startup data, at the observed 24 MHz internal clock. Host tests check the reload register and counter, and the ARM link check verifies the vector target. See [the SysTick trace](../h9-firmware-research/evidence/stock/aprs-systick.md).
- APRS serial command `0x11/4` is a GPIOB bit-5-gated raw USART2 write at 9,600 baud in the stock image; `aprs/h9_aprs_uart2_board.*` configures GPIOC pins 0/1 with alternate function 4 and performs bounded raw writes. The diagnostic entry configures GPIOB bit 5 as an output, initially drives it low, and binds the separate USART2 callback. This route is distinct from `0x00` packet-to-AFSK and framed USART1 replies. The USART2 peer and actual board behavior have not been measured.
- `aprs/h9_aprs_clock.*`, `h9_aprs_startup.c`, and `link_check_startup.S` add the stock reset-register writes, first-pass Cortex-M memory initialization, and a 48-entry vector table copied to RAM. The board entry now selects the stock 24 MHz internal-clock path with bounded readiness polling. SysTick, DMA channel 1, DMA channel 3, ADC1, USART1, and USART2 slots call replacement handlers; the NMI, HardFault, SVC, and PendSV entries and reserved zero slots now match the stock topology. Other interrupt bodies and board initialization remain unmapped.
- `aprs/h9_aprs_uart_receive.*` ports the stock USART1 receive handoff: two 512-byte buffers, a pending length and ready flag, serial-parser feed, buffer switch, and DMA rearm callback. It also converts the DMA remaining count on a UART idle event into the received length, following `0x0800423C`. `h9_aprs_uart1_dma.*` maps the observed USART1 and DMA channel registers, handles IDLE and DMA completion in fake-register tests, and binds their vector slots in the diagnostic image. An invalid DMA remainder now increments a diagnostic counter and rearms the available buffer so reception does not stall. `h9_aprs_uart1_board.*` ports the observed RCC, GPIO, 115,200-baud, and IRQ-enable setup with a system-clock input. The diagnostic entry calls that setup and polls received UART batches. Physical board testing remains open.
- `aprs/h9_aprs_uart1_tx.*` ports the stock per-byte USART1 transmit path: wait for transmit-empty, write DR, then wait for transmission-complete. The diagnostic entry supplies it to the controller for serial replies and forwarded packets. A bounded poll substitutes for the unported millisecond timer; a timeout latches a fault and makes later controller send operations report failure. Host tests exercise a version reply and both timeout flags. Physical UART output remains unverified.
- `aprs/h9_aprs_boot_banner.*` follows the stock startup text format over the same synchronous USART1 writer after board setup. Both the banner and command-`0x20` version reply now identify this replacement as `H9R0.01` while retaining protocol version `V1.0.0`. Host tests check the bytes and main-radio version parsing; the pinned APRS image check still verifies the vendor strings, and the link check rejects an ELF carrying the vendor software version. See [the boot-text trace](../h9-firmware-research/evidence/stock/aprs-boot-banner.md).
- The APRS controller now accepts stock serial command `0x0F` to enable or disable a one-byte TX status reply. Its TX service emits one event when the complete frame has entered the waveform queue, and the board loop can send `0x0F 01` at that point. This does not indicate RF completion. The replacement board loop also accepts a versioned, tokenized 10-second serial TX permit and emits replacement command `0x54` only after the final waveform sample drains. The main TX adapter matches that token to its source-level keyed RF transaction. The physical PTT and synth hooks are still unimplemented. See [the serial TX trace](../h9-firmware-research/evidence/stock/aprs-serial-tx-paths.md) and [replacement permit design](tx-permit.md).
- `aprs/h9_aprs_uart2_dma.*` ports the stock USART2 receive DMA channel and request mapping, with IDLE and DMA-complete handlers in vector slots 44 and 26. It forwards up to 1,020 bytes to the main radio as framed command `0x10`. This first pass uses one 1,024-byte DMA buffer and a 256-byte RX interrupt spill ring during forwarding. It drains the ring before rearming DMA; overflow is counted, and board timing is unverified. `h9_aprs_uart2_rx.*` retains a host-tested RXNE fallback but the diagnostic entry uses DMA. `h9_aprs_uart2_maintenance.*` now checks recovery every 1,000 loop passes and after more than 2,000 SysTick units since the last attempt, following the stock cadence. It uses the replacement's latched raw-TX fault and idle RX state in place of unmapped stock descriptor flags; this remains a replacement recovery policy. Host tests cover short and full chunks, DMA errors, request mapping, recovery after a latched fault, and the cadence/threshold/wrap cases. See [the USART2 receive trace](../h9-firmware-research/evidence/stock/aprs-uart2-receive.md) and [recovery trace](../h9-firmware-research/evidence/stock/aprs-uart2-recovery.md).
- `main/h9_main_serial_profile.*` constructs the observed 32-byte APRS serial-open argument with caller-supplied storage and event callback addresses. It also maps the stock pin tables: `0x20,0x21` selects UART channel 1 when free, at MMIO `0x001E2100` with interrupt `0x13`. The open routine's busy-channel fallback and the hardware read/write/interrupt implementation are not yet ported. See [the receive-ring trace](../h9-firmware-research/evidence/stock/aprs-main-uart-ring.md).
- `main/h9_main_serial_dma.*` ports the stock UART1 bulk-read ring helper at app+`0x0C5B4A`: it reads the produced/consumed counters, copies across a power-of-two DMA buffer wrap, and advances consumed. The function directly fits the session's serial-read callback. A host test delivers a wrapped command `0x21` frame through it to the main APRS parser. The UART1 target adapter provides source-level DMA setup and interrupt binding; the main board entry has not invoked it or verified it on hardware.
- `main/h9_main_uart1_irq.*` ports the independently decoded UART1 interrupt event ordering at app+`0x0C5EC8`: TX completion, full RX chunk, and partial RX chunk. It advances the DMA ring producer and invokes injected acknowledgement/event callbacks. Fake-register tests cover all three paths. The ROM DMA acknowledgement entry is mapped by `h9_main_uart1_dma_rom.*`; its internal behavior, vector binding, and board configuration remain untested.
- `main/h9_main_uart1_tx.*` ports the UART1 DMA transmit-start register order at app+`0x0C5E94`. Required board callbacks stand in for the two ROM DMA operations; the function refuses a nonempty transfer when they are absent. `h9_main_uart1_dma_rom.*` binds the three recovered ROM call addresses and observed TX/RX descriptor addresses to the UART1 link hooks. Fake-register tests check register order and hook routing. The caller must retain its source buffer through TX event 1. The ROM calls have not been tested on the radio.
- `main/h9_main_uart1_tx_queue.*` supplies that persistent storage using board-provided DMA-addressed slots. It copies the session's temporary serial reply before starting DMA, waits for UART event 1, then releases the slot and starts the next queued frame. Host tests cover ordering, a full queue, and a framed GPIO query whose reply reaches DMA storage. The queue is a replacement policy; board code must allocate DMA-capable slots, wire the IRQ event and ROM hooks, and handle rejected writes.
- `main/h9_main_aprs_uart1_link.*` connects the APRS session to the UART1 receive DMA ring, TX queue, and IRQ dispatcher. Its host tests deliver a framed GPIO query through simulated RX/TX interrupts and a checksum-valid GPS sentence through the RX path. Parsed GPS fixes and APRS burst activity now reach the application callback. The link queues a replacement-only, version-gated TX permit, waits for the APRS MCU's matching acknowledgment before permitting RF keying or packet queueing, then queues one packet and a permit clear; it accepts only the matching drained token before its bounded deadline. The link requires board-provided DMA storage and mapped ROM operations. It does not configure UART clocks, pins, DMA descriptors, an interrupt vector, or physical RF keying.
- `main/h9_main_uart1_board.*` ports the UART1 branch of the stock serial-open register sequence, including the occupied-UART check, DMA descriptor initialization order, IRQ registration, ring addresses, receive threshold, and baud-helper argument. Its hooks return success/failure so a failed setup is reported. `main/h9_main_uart1_baud.*` ports the independently decoded 48 MHz divider and conditional auxiliary register calculation with injectable MMIO, including the APRS profile's divider value of 1249. `main/h9_main_uart1_pins.*` ports the selector-1 pin-mux and GPIO writes for pins `0x20/0x21`. `main/h9_main_uart1_board_target.*` connects those operations with the mapped ROM DMA calls and the existing Pi32 IRQ registrar, with a single persistent UART1 vector context. It now copies the serial record's `+0x10` word to RAM `0x1E34C` before baud setup, matching the stock path and avoiding a stale auxiliary divisor. It checks clock and DMA-buffer prerequisites before touching MMIO. A host test opens the board and dispatches an RX interrupt through the vector. The main reset/board entry still does not call this adapter, and physical DMA, pin, and IRQ behavior remain untested. See [the board-open trace](../h9-firmware-research/evidence/stock/main-uart1-board.md).
- `main/h9_main_clock_timing.*` ports the next stock startup calculation at app+`0x0C51CA`. It chooses four bounded dividers from the selected five-word calibration record and writes the intermediate and final clock state used by later UART timing. The diagnostic board entry now calls it after a successful source match for the requested 240 MHz. Host tests cover all output fields, the target-rate minimum divider, and an unrepresentable divisor. See [the timing trace](../h9-firmware-research/evidence/stock/main-clock-timing.md).
- `main/h9_main_clock_apply.*` ports the first post-timing transition: ROM `0x02001634`, Pi32 `csync`, and two control-register writes around a 100-NOP delay. It also ports the final MMIO/source/latch/auxiliary sequence, two short delays, ROM `0x02001650`, and the helper's return. See [the apply trace](../h9-firmware-research/evidence/stock/main-clock-apply.md).
- `main/h9_main_clock_rom_mode.*` ports the following helper's two ROM `0x020000E2` calls and its four-entry stock band table `{7,8,10,14}`. The diagnostic board entry calls it after the first clock-apply transition and repeats the previously mapped selector before completing the remaining clock sequence. See [the ROM mode trace](../h9-firmware-research/evidence/stock/main-clock-rom-mode.md).
- `main/h9_main_post_clock_rom.*` ports the first board instructions after that helper returns: a signed low-byte check of ROM selector 49, conditional ROM selector 51 read, conditional `(17,1)` ROM control call, and two local-word initializers. It then reads selector 148, classifies the four-bit hardware field at `0x1F0200`, and writes mode `1`, `2`, or `4` to RAM `0x11B43`. The next branch reads ROM selector 192 and conditionally clears bitmap bit `0..55`, preserving the special flags for selectors 56 and 57. After the two-call ROM handoff, the replacement follows its four return-value branches through descriptor bit clears at `0x1E5000..0x1E50C0`, applies the four common descriptor masks, then handles the descriptor-32 conditional reset, descriptor-48 masks, and two flag-controlled MMIO blocks through app+`0x05FE5C`. The pinned stock bytes, descriptor table, target call order, and branch cases are checked by the link verifier and host tests. Later board setup remains unported. See [the post-clock trace](../h9-firmware-research/evidence/stock/main-post-clock-board.md).
- The next main timer-3 setup and ROM-selector branch through app+`0x05FE94` is modeled in `main/h9_main_post_clock_rom.*`, including an IRQ-3 vector/control target binding. The diagnostic board entry invokes it after the MMIO gate stage.
- `main/h9_main_timer3_irq.*` models the timer-3 handler's counter, acknowledgement, ROM clock-check, and mismatch-fault branches from app+`0x03126C..0x0312D2` for host tests. The Pi32 ISR is direct assembly in `main/h9_main_startup.S`, preserving the stock register state at its ROM calls and the interrupt save/restore order. The wider clock contract and live interrupt behavior have not been verified on hardware.
- `main/h9_main_irq_registration.*` ports the stock 64-entry interrupt mapping and shared vector/control update used by IRQ 3 and, immediately afterward, IRQs 0 and 1. The diagnostic entry installs IRQs 0 and 1 with `main/h9_main_exception.*`: their RAM handler saves the stock 24-word processor frame, records it in RAM, and halts. Stock prints a longer crash report; that printer is still unported. The RAM disassembler now annotates candidate flash targets for relative calls under the Pi32 32 MiB call window.
- `aprs/h9_aprs_gpio_board.*` ports the observed command-`0x11` GPIOB bit-5 set, clear, and input read through the `0x50000400` register bank. Its board initializer enables the GPIOB clock, sets bit 5 as an output, and initially clears it as stock `0x080029F8` does. The pin's external circuit remains unverified.
- `aprs/h9_aprs_adc_board.*` reads the ADC data register at `0x40012440`, derived from stock sample-state initialization, and ports all three observed IRQ 12 branches: sample, watchdog, and end-of-sequence. Its replacement vector slot 28 calls the controller for sample flags and preserves the other branches' state and flag effects; both stock branch callbacks are empty. It pulses ADC reset, enables its clock, calibrates with a bounded wait, configures PA1/channel 1 and the recovered ADC register masks, then enables conversion and IRQ 12 after a ready check. `aprs/h9_aprs_wave_output.*` configures GPIOA pins 4–7 and binds waveform output callbacks to the output-data register at `0x50000014`; it also configures and initially clears PA0 as observed in the stock startup chain. The controller normalizes each ADC sample, demodulates before optional TX output, reverses the waveform high nibble, and preserves the output register's low nibble. See [the ADC IRQ trace](../h9-firmware-research/evidence/stock/aprs-adc-irq.md) and [startup trace](../h9-firmware-research/evidence/stock/aprs-startup.md); actual sample timing and routing still need board verification.
- `aprs/h9_aprs_sample_timer.*` ports the stock `0x08002824` timer setup at `0x40012C00`: enable its RCC clock, set prescaler 11 and period 209, write the observed `0x20` CR2 field, and start the counter. The diagnostic entry starts this timer before ADC initialization. Its relationship to the actual ADC sample cadence is inferred from the setup chain and still needs a board measurement.
- `aprs/h9_aprs_runtime_state.h` groups the controller, TX service/runtime gate, UART receive buffers, and current ADC/GPIO/UART/timer register bindings. The diagnostic board entry reserves one instance, starts the candidate sample timer, performs the recovered ADC startup sequence, and processes UART receive batches. It connects the TX callbacks and reads the demodulator preamble/frame state. The replacement permit protocol is connected in source, but has no hardware validation. The link check accounts for current static RAM use without claiming a functional APRS controller.
- `aprs/h9_aprs_board_runtime.c`, `h9_aprs_startup.c`, and the target-only `h9_aprs_freestanding.c` are now part of the APRS cross-compile archive. The board loop, reset memory setup, and required `memcpy`/`memset` implementations therefore compile in the ordinary target source pass; the vector assembly and diagnostic linker envelope are added by `link_aprs_check.py`. This is source organization toward a full image, not evidence that the board wiring or updater works.
- `aprs/h9_aprs_bitstream.*` ports the recovered-bit framing path at `0x080055C4`, including HDLC flag recognition, stuffed-zero removal, byte assembly, and the original `0x1B` escape convention.
- `aprs/h9_aprs_ring.*` ports the receive-byte queue's full, empty, and wrap behavior from `0x080053E0..0x08005470`; the selected Ghidra report is in `../h9-firmware-research/evidence/aprs-bitstream-helpers.txt`. Replacement single-producer/single-consumer atomic indices now separate the ADC ISR's tail updates from the main loop's head updates. On overflow, the ISR increments a generation counter and aborts its partial bitstream; the main loop discards queued partial bytes and resets its frame parser before accepting a new frame. The independent AFSK test forces overflow and then recovers a complete packet at both sample rates. This is a source concurrency policy, not a measured stock or radio behavior.
- `aprs/h9_aprs_demod.*` ports the five-sample delay line, correlation/filter, timing recovery, and NRZI bit extraction at `0x08002308`, using the initialization at `0x08002604`. The controller now runs this source on each ADC sample before updating the TX DAC.
- `tests/test_external_afsk.c` independently modulates a fixed AX.25 UI frame as continuous-phase 1,200/2,200 Hz AFSK, converts it to synthetic 10-bit ADC readings, and feeds the APRS controller's ADC callback. The controller demodulates, validates FCS, frames the received packet for UART, and the main firmware source parses and decodes the forwarded frame. It recovers the original `APRS > KD9CRQ` packet at both 9,600 samples/s and the `24 MHz / 12 / 210` timer-derived rate. This checks source integration beyond the replacement TX loopback but does not substitute for captured RF audio, measured ADC scaling, physical UART behavior, or radio validation. See [the AFSK validation note](../h9-firmware-research/evidence/stock/aprs-demod-afsk-validation.md).
- `aprs/h9_aprs_frame.*` ports the byte-queue parser at `0x080051FC`: HDLC flags, escaped bytes, a 792-byte receive limit, and the stock CRC-16/X-25 residue check. The controller can drain the demodulator queue and forward a valid packet to the main radio as serial command `0x00`, excluding its two FCS bytes. The source-input verifier checks all 256 CRC table entries against the APRS image.
- `aprs/h9_aprs_oscillator.*` ports the 9-bit phase accumulator and quarter-wave table in the tail of `0x08002468`. The table is copied from stock APRS-image offsets `0x40D8..0x4157`; the 0x40 and 0x75 phase steps are tested against vectors independently calculated from those bytes.
- `aprs/h9_aprs_tx.*` ports the transmit queue, leading/trailing flag counts, eight waveform samples per bit, NRZI tone changes, and stuffed-zero insertion from `0x08002664` and `0x08002468`. The permit-gated TX service owns this scheduler for DAC output. The controller has no second waveform queue or fallback path when the board callback is absent.
- `aprs/h9_aprs_tx_packet.*` ports `0x0800530C`/`0x0800534C`: an opening flag, payload with `0x1B` escapes for reserved bytes, complemented FCS low byte first, and a closing flag. Its resumable pump handles the stock 64-byte transmit queue without a blocking wait. The byte stream round-trips through the receive frame parser in host tests. The serial command `0x00` is now dispatched to a synchronous board callback for the stock `0x08005760` send path.
- `aprs/h9_aprs_tx_gate.*` ports the `0x0800211C` random generator and the persistence and receive-event decisions from `0x08005760` into a cooperative step. A corrected pointer trace places the gate's `+3` in-frame, `+4` six-flag preamble, and `+0xE0` queue-error fields in the demodulator/waveform object, not the receive-frame buffer. The diagnostic entry now reads an atomic demodulator snapshot for those inputs; the queue-overflow generation approximates the stock error-event timing. This does not identify a physical carrier-sense GPIO; see [the TX-gate state trace](../h9-firmware-research/evidence/stock/aprs-tx-gate-state.md).
- The independent AX.25 receive test now passes a frame through the replacement ADC sampler, APRS decoder, UART framing, and main-radio parser at the recovered 9.52 kHz sample rate with a reduced signal amplitude, DC offset, and deterministic noise. This is a software stress case; actual RF audio capture remains necessary for hardware validation.
- `aprs/h9_aprs_tx_service.*` owns a serial TX request and waits for the gate in the main loop. It then publishes a packet to the ADC interrupt, which alone initializes and advances the packet encoder and waveform state. Atomic phase and packed lead/tail settings provide the handoff; an interrupted abort is acknowledged by the ADC callback before packet storage can be reused. This assumes the ADC interrupt preempts the single main loop and no other interrupt calls the service. Host tests cover cancellation before and during a waveform, and loop an 80-byte packet through the ported receive demodulator and frame parser. This is software evidence, not a measured RF or ADC/DAC test; worst-case ADC interrupt timing remains unmeasured.
- `aprs/h9_aprs_tx_runtime.*` connects serial command `0x00` to that service and its output to the controller's per-sample waveform callback. It requires explicit RF-ready authorization before accepting a packet and on every cooperative step; losing authorization cancels a pending request or requests interrupt-owned waveform shutdown. The diagnostic APRS board entry derives this authorization from the bounded replacement command `0x53` permit. The main board still lacks verified RF/PTT hooks, so this is not a validated transmit path. A host test checks rejection, queueing after authorization, the gate, sample callback, and abort paths.
- `main/h9_main_aprs.*` implements source-level TX and RX-audio policy. The TX enum comes from main app+`0x05C2D4` and clone offset `0x3117`. B-Lock retains the stock current-side guard, while the replacement policy requires both selected side and programmed TX frequency to match B before keying. The plan records when an Auto mode must restore the prior side after sending, matching the stock selector's temporary side switch. The recovered Dual Watch side decision now selects A or B's stored frequency pair for working RF state. TX setup's input chooser now handles both the working pair and the optional override pair, returning the accompanying shift mode; stock flag `0x12BBA` reverses which word it takes from each pair. Other channel settings remain unported. See [the TX setup trace](../h9-firmware-research/evidence/stock/main-aprs-tx-setup.md). The mute policy leaves the selected APRS decoder feed enabled while asking **both** speaker and Bluetooth paths to be silent only for an explicitly classified APRS-burst interval; ordinary voice on the same channel remains audible. No hardware classifier or gate has yet been identified that can enact both requests without cutting decoder input.
- `main/h9_main_tx_service.*` adds a replacement TX transaction around that policy. It reads the working RF side, rejects a Lock request on the other side, requires a frequency precheck before unkeyed RF preparation, and checks the prepared side, frequency, Dual Watch route, and working frequency pair before it permits packet send. `h9_main_rf_route_prepare` gives the board adapter a source-level way to aim both selectors at the planned side before RF setup and restore them afterward, while retaining the stock Lock guard. The `prepare_tx` callback receives the full plan so it can distinguish Auto from Lock; the host fake adapter uses the route helper in both modes. The service requests the replacement APRS transmit permit only after readback. After its acknowledgment, it rereads position, frequency policy, synth state, and the RF route before the only keying callback. A changed field rejects the send, then cleanup stops TX, clears the permit, and restores RX. The stock selector at app+`0x05C2D4` orders RF selection, TX setup at app+`0x05AC68`, and packet send at app+`0x05BE2C`; the exact stock point where PTT first asserts is still unresolved. A live board adapter must serialize Dual Watch updates and enforce the unkeyed-setup contract, report the actual programmed RF state, transmit and clear the permit, and implement the cleanup calls. No live board adapter is connected yet, and firmware state cannot replace a measured RF bench check. See [the route-readback trace](../h9-firmware-research/evidence/stock/main-aprs-route-readback.md) and [permit design](tx-permit.md).
- `main/h9_main_tx_frequency.*` ports the stock repeater shift conversion, the first TX band/region gates, and the subsequent VHF/UHF range check at app+`0x056612`. The source-image verifier compares all 30 region-table words and the four copied limit words with the pinned app. The initial VHF range is 136–174 MHz and UHF range is 400–520 MHz; the combined check selects the range from the stock 300 MHz band split. Other stock TX setup and physical RF output remain unported. See [the frequency trace](../h9-firmware-research/evidence/stock/main-tx-frequency-precheck.md).
- `main/h9_main_tx_service.*` now requires a route reservation for the entire APRS TX transaction. It holds the reservation from the first logical-side read through preparation, side/frequency readback, the packet callback, TX stop, and receive-state restoration. This closes the service-level gap where Dual Watch could change its separate route byte between checking B-Lock and keying A. A live reservation mechanism and RF board callbacks remain unimplemented. See [the route readback trace](../h9-firmware-research/evidence/stock/main-aprs-route-readback.md).
- `main/h9_main_tx_adapter.*` connects that transaction to the ported route-state memory and the new UART permit/packet/completion sequence. Its board hooks isolate the still-unverified RF preparation, synthesizer readback, PTT, route lock, clock, and scheduler yield. The adapter bounds polling by both a millisecond deadline and a step count. A host integration test covers B-Lock with Dual Watch's route initially on A, permit and waveform acknowledgments, missing acknowledgment, and a failed PTT stop. If PTT stop cannot be confirmed, it leaves the route reservation held and reports cleanup failure. No physical hook implementation or RF measurement exists yet; this source must not be treated as a live transmitter adapter.
- `main/h9_main_tx_ram_policy.*` binds the observed stock RAM fields for beacon position eligibility, region mode, band flags, and live frequency limits. The TX adapter reads them through this module while its route reservation is held. It rereads the fields to reject a snapshot that changed during capture; that check is not an atomic lock against concurrent writers. Host tests exercise the binding and policy decisions.
- `main/h9_main_rf_output.*` ports the stock register effects at app+`0x050720`, `0x058E58`, and `0x058E80`: bit 0 and bit 8 of MMIO `0x001E5000`, and duty/enable writes at `0x001E0600`. The independent Pi32 decode corrected a Ghidra error in the bit-8 clear branch. Fake-register tests and the Pi32 link pass, but the output roles are not measured and no board entry calls these functions. See [the register trace](../h9-firmware-research/evidence/stock/main-rf-output-controls.md).
- `main/h9_main_audio_arbiter.*` ports the stock receive-audio gate's early request, hold-bit, and source selection decisions at app+`0x05224C`. A held bit-2 state prevents an ordinary OFF request; source 13 can clear that hold. Sources 1, 8, 11, 12, and 13 use direct speaker decisions, while other sources require the unmapped radio-state policy through a callback. The module produces a plan only; it is not connected to the physical speaker writer or Bluetooth path. See [the audio arbiter trace](../h9-firmware-research/evidence/stock/main-audio-gate-arbiter.md).
- `main/h9_main_rf_registers.*` ports the beginning of stock RF frequency programming at app+`0x058F94`: band-specific signed calibration, division by 10, and low/high writes to RF commands `0x38`/`0x39`. It also maps stock path/mode values to register `0x74` or `0x75`, including the TX path's mode-2 `0xDF22` word. The stock AF selector at app+`0x0592E4` is ported as register `0x47 = 0x6040 | (mode << 8)` with an injected write callback. `h9_main_rf_setup_side` now ports the Dual Watch choice between RF side-setup routines, using the route byte when enabled and channel-state bit 7 otherwise. Host tests verify those choices and the RF register values. The AF register's effect on APRS decoding and Bluetooth audio, subsequent RF setup, ROM register transport, and keying boundary remain unported or unverified. See [the RF register trace](../h9-firmware-research/evidence/stock/main-rf-register-setup.md), [AF selector trace](../h9-firmware-research/evidence/stock/main-rf-af-selector.md), and [APRS TX setup trace](../h9-firmware-research/evidence/stock/main-aprs-tx-setup.md).
- The receive-setup helper at app+`0x05CB2C` is also ported through the RF register and audio request callbacks: it writes `0x30`, selects AF mode 1 through `0x47`, writes `0x70`, and conditionally requests source-4 audio OFF. Its exact stock slice and the callback order are verified. The route from this AF setting to the separate APRS ADC and Bluetooth audio output remains unmeasured.
- The same main module ports the channel-state bit update at app+`0x05A538..0x05A676`: when the working frequency matches A or B, it clears or sets bit 7 of RAM `0x00011B50`. This bit feeds the stock APRS TX selector. Its caller's timing relative to Dual Watch and the highlighted display line remains unverified; a B bit alone does not prove the RF frequency emitted during a beacon.
- The same main APRS module ports the packet-path RX-channel comparison at app+`0x086166..0x08619C`. This comparison maps the working RX frequency to A or B before checking `APRS RX CH`; `Off` bypasses this local comparison, which does not establish that APRS reception is globally enabled in that setting.
- `main/h9_main_ax25.*` ports the address and packet assembly at app+`0x05B24A`/`0x05B298`: shifted six-byte callsigns with zero padding, SSID and final-address bits, optional digipeaters, control `0x03`, default PID `0xF0`, and the observed 512-byte packet limit. It also decodes received destination/source/digipeater addresses and at most 256 information bytes from app+`0x05D9A6`/`0x05D9F6`, and applies the PID and nonempty-information filter observed in the command-`0x00` dispatcher. It accepts clean source structs instead of copying the stock RAM layout.
- `main/h9_main_aprs_wire.*` ports the outgoing serial envelope at app+`0x05B32E`: five leading `0xC0` delimiters, command byte, escaped payload, and three trailing delimiters. The stock wrapper calls it with command `0x00` and a 2,048-byte output buffer, then passes the result to a UART callback. A host test now composes the main AX.25 builder and serial envelope with the APRS controller, TX service, waveform generator, demodulator, and receive parser. The decoded packet equals the built packet in ideal software loopback.
- `main/h9_main_aprs_aux_control.*` ports the stock main-to-APRS command `0x11/1` or `0x11/2` when an auxiliary setting is applied and the APRS serial handle is open. A cross-firmware host test confirms that these commands raise or lower the APRS-controller GPIOB5 gate for raw USART2 traffic. The setting name and external GPIO circuit remain unverified; this gate is not used as an RF transmit permit. See [the main auxiliary GPIO trace](../h9-firmware-research/evidence/stock/main-aprs-aux-gpio-command.md).
- `main/h9_main_aprs_receive.*` ports the main radio's incoming APRS-module UART frame state machine at app+`0x087340`: delimiter and command handling, `0xDB` escapes, a 1,024-byte payload buffer, and overflow reset. The stock dispatcher at app+`0x0870BC` branches on commands `0x00`, `0x10`, `0x11`, `0x21`, and `0x51`. Command `0x00` now decodes and filters an AX.25 packet for later APRS UI handling. Host tests pass a received packet and an APRS-controller version response through this frame receiver. The hardware UART producer remains to be ported.
- `main/h9_main_aprs_control.*` validates received command `0x51` with the stock CRC-16/CCITT-FALSE variant and identifies the eight group/opcode pairs observed in the stock dispatcher. A volume reducer handles the stock group-`06` up/down and held-adjustment transitions over a caller-supplied `0..30` level, reporting timer and boundary-feedback effects. It now tracks the saved setting separately from the live level and reports the stock reconfiguration, audio-apply, and UI calls after a change, subject to the observed output guard. The `07 03` reducer clears the observed flags, starts a 50-unit timer only on the first pending transition, and models the callback's handle clearing and branch. The independent Pi32 decode corrects Ghidra's erroneous timer-handle address to RAM `0x12C60`. It does not run board, audio, UI, or timer side effects; the APRS session still emits raw actions for a future board adapter. The exact meaning of several pairs remains under investigation. See [the command trace](../h9-firmware-research/evidence/stock/aprs-main-control-command.md).
- `main/h9_main_uart_ring.*` drains the incoming 2,048-byte UART ring in at most 1,024-byte batches, copying a snapshot before advancing the consumer index and feeding the frame parser. A batch producer now follows the stock capacity check at app+`0x078F76`: it accepts a whole read or drops it without a partial copy. Counting dropped bytes is a replacement policy; the stock overflow flag's later behavior is still unknown.
- `main/h9_main_aprs_session.*` composes the ring, parser, AX.25 decoder, GPS collector, version state, control dispatch, and GPIO query reply. Its poll asks a board-supplied serial read callback for up to 1,024 bytes, handles the stock `PY 02 01` short-control prefix, then drains the ring and handles framed commands `0x00`, `0x10`, `0x11`, `0x21`, and `0x51`. The corrected `PY` checksum sums all five preceding bytes, including `P`; host tests reject the old checksum and exercise the ready flag and `THD` reply. The stock serial-open record and its three candidate UART register blocks are mapped in [the receive-ring trace](../h9-firmware-research/evidence/stock/aprs-main-uart-ring.md). A board adapter must still supply UART read/write behavior and apply receive-channel policy to packet callbacks.
- `main/h9_main_aprs_metadata.*` handles three more received commands: `0x10` collects `$`-started GPS sentences across frames and emits complete newline-terminated sentences, following app+`0x086EF4`; `0x21` parses the APRS module and protocol version replies from app+`0x0870BC`; and `0x11` builds the observed `{3, 0}` response when queried with first payload byte `3`. Tests use responses from the separate APRS controller source and decode the `0x11` reply with its serial parser. Persistent version storage, UART write callback, and UI refresh still need implementation.
- `main/h9_main_gps_fix.*` parses checksum-valid RMC/GGA sentences into time, date, fix state, and signed coordinates; the APRS session now exposes a parsed-fix callback alongside its raw sentence callback. This is a bounded first-pass replacement policy because the stock GPS parser at app+`0x086656` has unresolved instructions in the Pi32 decompiler. See [the GPS trace](../h9-firmware-research/evidence/stock/main-gps-fix.md). It is not yet connected to the radio's UI or stored GPS data model.
- `main/h9_main_speaker_gate.*` ports the stock app+`0x052228` GPIO3 and state-byte writer for the built-in speaker. The exact pin-bank pointer table and writer bytes are checked against the pinned stock image. Bluetooth audio bypassed this gate in the prior hardware probe, so this module is not the requested APRS mute and is not yet connected to the receive policy. See [the speaker boundary trace](../h9-firmware-research/evidence/stock/main-speaker-gate.md).
- `main/h9_main_audio_policy.*` ports the stock app+`0x052102` speaker-routing decision from raw RAM state into the existing audio arbiter callback. Its full instruction slice is pinned to the stock image and host tests cover representative branches plus arbiter composition. The live state snapshot and Bluetooth output route are still missing. See [the audio-arbiter trace](../h9-firmware-research/evidence/stock/main-audio-gate-arbiter.md).
- `main/h9_main_audio_status.*` ports stock app+`0x01677C` and `0x01682C`, selecting one of the two active audio records and supplying the byte consumed by the speaker policy. The ambiguous-record fallback uses an injected six-byte key comparator because its ROM implementation has not been ported. The stock instruction slice is pinned and tested. This does not identify the live receive side or Bluetooth output gate.
- `main/h9_main_beacon.*` ports the timed-beacon counter, interval threshold, and state/priority guard from main app+`0x082EB2`. Its stock caller at app+`0x08747C` invokes the timer only when RAM `0x11AB0` is at most `1`, `0x11AC6` equals `3`, and both `0x11AFB` and `0x11AFE` are zero; a skipped call retains the counter. Two operating loops yield one ROM scheduler tick after invoking the caller, but the tick duration and flag meanings still need tracing. It also ports the GPS-position preflight from the APRS TX selector: source mode `1` requires a valid fix and at least one nonzero coordinate word. The TX service requires this snapshot before RF preparation.
- `main/h9_main_boot_record.*` and `main/h9_main_boot_validator.*` port the retained startup record's 16-bit check and the hardware streaming sequence at app+`0x05E494`/`0x04CAEC`. The validator has injectable hooks for tests and Pi32 ROM/MMIO bindings. The diagnostic reset path now consumes the record at `0x2FF88` after the stock interrupt-mask step and stores the result at `0x11C3C`; the board record's physical purpose remains unverified. See [the startup trace](../h9-firmware-research/evidence/stock/main-startup.md).
- `main/h9_main_task_registry.*` reconstructs the 27-name startup task registry after app+`0x05FFAC`, including `app_core` and `my_app`. Its launch helper now passes the selected record's three fields and the requested entry/argument/name to ROM `0x02003102` through an injectable target wrapper, returning `-14` for missing names or failed creation. The board entry also calls ROM `0x02002CAE` with RAM `0x12850` before those two tasks. The ROM field meanings, task bodies, and later scheduler startup remain unported, so the diagnostic board entry does not yet start them. See [the task-registry trace](../h9-firmware-research/evidence/stock/main-task-registry.md).
- `main/h9_main_board_tail.*` ports the observed ROM setup, task-launch, and scheduler-handoff order following the memory-window setup. It requires replacement entrypoints for `app_core` and `my_app`; the diagnostic board entry stops before this tail until their bodies and ROM effects are established.
- `main/h9_main_pin_registry.*` ports the first five `my_app` pin-helper calls, their ten-entry RAM registry, and the preceding 22-word all-ones registry reset inside the direct `app_core` setup call. `main/h9_main_pin_actions.*` binds the five calls to the observed GPIO bank words and masks; it rejects pin 38 and the alternate high-pin branches. The rest of that setup call and `my_app` are not yet ported. See [the early pin trace](../h9-firmware-research/evidence/stock/main-my-app-early-pins.md).
- `main/h9_main_core_samples.*` ports the next `app_core` setup block: two ADC mode selections, 16 stored samples, a ten-sample 16-bit average, and the ready flag. `main/h9_main_adc_mode.*` now binds the two exact selector modes used here to the observed ROM calls and MMIO writes, rejecting other modes. The vendor Pi32 disassembler resolves the sample poll as control bit 7, correcting a conflicting Ghidra interpretation. The wider setup function remains unported. See [the sample trace](../h9-firmware-research/evidence/stock/main-core-adc-samples.md) and [mode trace](../h9-firmware-research/evidence/stock/main-adc-mode-selector.md).
- `main/h9_main_core_post_samples.*` ports the next `app_core` sequence: callback registration, a RAM module pointer, a direct 164-byte clear, a two-word self-linked list, `dev_mg` task creation through the replacement task registry, conditional follow-up, and final state-byte dispatch. Two stock application wrappers collapse to their observed ROM calls. Remaining calls are explicit hooks because their replacement implementations are not yet available. The `dev_mg` task must supply a replacement entrypoint. Host tests check call order, arguments, zeroed storage and list state, and both task-creation outcomes. See [the post-sample trace](../h9-firmware-research/evidence/stock/main-core-post-samples.md).
- `main/h9_main_dev_mg_task.*` ports that task's observed entry sequence: set its ready byte, initialize the built-in device table, register `sdfile`, `fat_nor`, and `res_nor` in order, acknowledge the queue descriptor, and reuse a 32-byte buffer in its blocking receive loop. The device registry and ROM queue operations remain callbacks; there is no callable target entrypoint or scheduler connection yet. See [the task entry trace](../h9-firmware-research/evidence/stock/main-dev-mg-task.md).
- `main/h9_main_volume_registry.*` ports the called registration helper's named-driver lookup, duplicate list check, 24-byte Pi32 node layout, open-and-insert behavior, sequence assignment, and stock result codes. It retains a failed-open node as stock does. Allocation, locks, and device open calls require board hooks, and the fourth stock driver record has not been reconstructed. Host tests cover all observed result branches and list order. See [the device-manager trace](../h9-firmware-research/evidence/stock/main-dev-mg-task.md).
- `main/h9_main_core_list.*` ports one `app_core` initializer from app+`0x0462F8`: a two-word self-linked list head at RAM `0x119EC`. The diagnostic entry does not yet call it because the `app_core` task has not been connected. See [the task-registry trace](../h9-firmware-research/evidence/stock/main-task-registry.md).
- `main/h9_main_core_init.*` reconstructs the first `app_core` initializer table, its call order around the direct setup call, and the three 28-byte event records walked by app+`0x02C514`. It dispatches through hooks so stock addresses can later be mapped to replacement functions. The task, remaining initializer bodies, and event callbacks remain unconnected to the board entry. See [the task-registry trace](../h9-firmware-research/evidence/stock/main-task-registry.md).
- `main/h9_main_channel_defaults.*` ports the verified opening loops of another `app_core` initializer at app+`0x048A46`: two default records in each of two RAM blocks, their 24-byte stock template, and the associated state bytes. The saved-channel import that follows those loops remains to be ported. See [the task-registry trace](../h9-firmware-research/evidence/stock/main-task-registry.md).
- `main/h9_main_sys_event.*` ports another `app_core` initializer from app+`0x02D4D8`: the RAM `0xC070` guard, linked event entries, optional event buffer, and `sys_event` task request. It also ports the visible task prelude at app+`0x02D542` through its ROM event-loop handoff. Host tests cover allocation failures, link pointers, task parameters, and prelude values. The actual event loop, target allocator, and `app_core` caller remain unported; the diagnostic entry does not invoke this code. See [the task-registry trace](../h9-firmware-research/evidence/stock/main-task-registry.md).

The framing and command source follows observed behavior where established. The mute and post-setup TX verification are **new desired behavior**, not claims that stock firmware already performs them. Unsupported commands are ignored in this first pass. The full main UI, channel store, RF driver, Bluetooth driver, boot setup, and APRS hardware integration are still absent. The receive chain has passed ideal software loopback but has not been validated with recorded packet audio. The APRS status command's 48-byte content remains a hardware callback because its fields have not been fully reconstructed. The vendor Pi32 compiler now builds the current source modules, but no production link map has been validated for the radio. The CMake test executable is host-only; the cross-compile script below creates target objects and archives, not linked firmware.

## Build and test on Windows

With Visual Studio Build Tools installed:

```powershell
cmake -S firmware-research/replacement-source -B firmware-research/replacement-source/build -G "Visual Studio 17 2022" -A x64
cmake --build firmware-research/replacement-source/build --config Debug
ctest --test-dir firmware-research/replacement-source/build -C Debug --output-on-failure
```

On Linux, select an available native C compiler and omit the Visual Studio generator and `-A x64`. These tests do not access the radio.

## Cross-compile the source modules

`build_target_objects.py` compiles the current source sets separately and records the compiler, flags, source hashes, object hashes, and archive hash in `build-manifest.json`. It runs on Windows or Linux. The APRS build uses [Arm GNU Toolchain](https://developer.arm.com/Tools%20and%20Software/GNU%20Toolchain)'s `arm-none-eabi-gcc` and `arm-none-eabi-ar`. The conservative `cortex-m0`/Thumb setting only establishes a compatible ARM object build; the precise APRS CPU variant has not been confirmed. The main build requires JieLi's **Pi32v2 r3** clang and LTO archiver. The [official JieLi SDK build](https://doc.zh-jieli.com/SDKWiki/master/Jieli-Tech/fw-AC63_GP_MCU/5-build-tools/1-build-system-workspace.html) uses `C:/JL/pi32/bin` on Windows and `/opt/jieli/pi32v2/bin` on Linux, with `-target pi32v2 -mcpu=r3 -flto -Oz`.

```powershell
py -3 firmware-research/replacement-source/build_target_objects.py aprs --compiler 'C:\path\to\arm-none-eabi-gcc.exe'
py -3 firmware-research/replacement-source/build_target_objects.py main --compiler 'C:\JL\pi32\bin\clang.exe'
py -3 firmware-research/replacement-source/build_target_objects.py main --no-lto --compiler 'C:\JL\pi32\bin\clang.exe'
```

On Linux, use `python3` and omit `--compiler` if the respective compiler is at its documented path or on `PATH`. `H9_ARM_GCC` and `H9_PI32_CLANG` can also set explicit compiler paths. The resulting `.o` files and `.a` archive are under ignored `build/target-aprs`, `build/target-main`, or `build/target-main-native`. The default main build emits LLVM bitcode for the vendor LTO linker; `--no-lto` emits ELF32-pi32v2 objects with machine ID `0xF1` for disassembly and later linker work. Both formats are checked before archiving. The APRS archive was built and checked with Arm GNU Toolchain 15.3.1 on Windows. The main archive was built in both LTO and native modes with JieLi clang 4.0.1 on Windows. **Neither archive is a bootable or flashable image**: startup, linker memory map, peripheral drivers, and final image packaging are still missing.

To check the source constants against the private stock images, run `py -3 firmware-research/replacement-source/verify_source_inputs.py <path-to-stock-app.bin> <path-to-APRS-1.0.15.bin>` from the repository root. This reads both files and validates their pinned hashes, the main TX selector call, side and working-frequency copy instructions, all 30 region-mode frequency words, four VHF/UHF limit words, all 128 waveform-table bytes, and the APRS CRC table. It does not create or flash an image.

## APRS diagnostic link check

`link_aprs_check.py` compiles all APRS modules, forces every object into one ARM ELF, and writes a linker map and manifest under ignored `build/link-check-aprs/`. Its diagnostic linker script uses the stock image base `0x08002000`, stock image length 17,304 bytes, and stock initial stack pointer `0x20001A50` as conservative address bounds. The optional stock-image argument rechecks those values against the pinned private image hash. This is a **link check**, not a complete firmware build: its 48-entry vector table and first-pass reset path perform the observed early register writes, initialize ordinary `.data`/`.bss`, and relocate vectors. Its board entry configures partial UART, timer, ADC, and GPIO paths; RF controls, other interrupts, error reporting, and full APRS application remain absent.

```powershell
py -3 firmware-research/replacement-source/link_aprs_check.py --compiler 'C:\path\to\arm-none-eabi-gcc.exe' --stock-image 'C:\path\to\APRS-1.0.15.bin'
```

Add `--emit-raw` to export `h9_aprs_diagnostic_NOT_FOR_FLASH.raw` for offline layout inspection. It is padded to the stock 17,304-byte length and checked against the ELF vector table. The APRS flasher's `inspect` command accepts this raw file for structural inspection, but its checks cannot establish that the code runs on the radio. The manifest records `flashable: false`; the flasher requires a matching `aprs_release_image` manifest and a byte-verified stock recovery image for any modified image, so this diagnostic manifest cannot authorize a write. No release-image producer exists yet.

The Windows Arm GNU Toolchain 15.3.1 check linked 33 APRS target source objects from the normal archive, confirmed all 48 vector entries and the SysTick/DMA1/3/ADC/USART1/2 IRQ slots, and placed 5,360 bytes of state in `.bss`. It reports 1,184 bytes between the end of `.bss` and the captured stack top. `analyze_aprs_stack.py` follows direct calls from reset, the board loop, and active interrupt handlers using compiler `.su` frames; the current longest direct path is 392 bytes, rising to 432 bytes with known board callbacks. A main-loop path interrupted by the largest known IRQ path adds an 8-word exception frame plus possible alignment padding, totaling 684 bytes, compared with the largest individual frame of 136 bytes. It identifies 26 indirect call sites, some with mapped targets, and nine linked helpers without `.su` records. The link check rejects a known direct, callback, or one-interrupt path over the available RAM but does not prove stack safety: remaining callbacks, multiple interrupt nesting, missing frames, and unported board code remain. See [the startup trace](../h9-firmware-research/evidence/stock/aprs-startup.md), [UART receive trace](../h9-firmware-research/evidence/stock/aprs-uart-receive.md), [UART transmit trace](../h9-firmware-research/evidence/stock/aprs-uart-transmit.md), [boot-text trace](../h9-firmware-research/evidence/stock/aprs-boot-banner.md), [GPIO trace](../h9-firmware-research/evidence/stock/aprs-gpio-board.md), and [sample-input/output trace](../h9-firmware-research/evidence/stock/aprs-wave-output.md).

`link_main_check.py` compiles **all current main modules** into native Pi32v2 objects and uses JieLi's `pi32v2-ld` to produce an ELF, map, and manifest under ignored `build/link-check-main/`. JieLi clang's default link invocation selects `pi32-ld`, which rejects Pi32v2 objects, so the script explicitly selects the matching linker. The first-pass reset entry in `main/h9_main_startup.S` copies the bootloader's launch fields into RAM before setting both stack pointers to `0xE290`; its 42-byte copy routine and 12 stack-setup bytes are checked against the pinned stock app. The C runtime clears the stock `0xE2E0..0x1E4D4` interval, copies `.data`, copies authored `.ram_code` into RAM address zero, and applies the stock `icfg` mask before reaching the partial board bootstrap. The authored `.ram_code` has a 64-byte zero prefix followed by a bounded byte-transfer routine at RAM `0x40`, matching the stock routine's location and register operations but using a new C interface. The rest of the stock image's 55,928-byte RAM region has not been reconstructed. The link check verifies archive inclusion, no undefined symbols, Pi32v2 ELF type and entry address `0x01E00100`, the RAM routine's placement and zero prefix, the `icfg` register-operation bytes, and that C data/BSS and RAM code fit the observed intervals. This check does **not** create a flashable firmware `.bin` or `.fw` image.

Add `--emit-raw` to place the linked `.text`, `.ram_code`, and initialized `.data` at their checked load addresses in a stock-size `h9_main_diagnostic_NOT_FOR_FLASH.raw` file. The remaining bytes are `0xFF`. This is for offline layout inspection; the board entry stops after the first four-channel clock measurement, register-selection, calibration-band selection, arithmetic, source/divider search, timing calculation, the complete observed clock helper, the post-clock ROM handoff, descriptor-mask paths, flag-controlled MMIO writes, timer-3 setup, selector branch, IRQ 0/1 registration, ten clock-gate handshakes, memory-source mapping, two post-map register phases, and the memory-window helper through app+`0x05FFAC`. Much of the RAM code and device behavior is unported, and the manifest retains `flashable: false`. Do not feed this raw output to the firmware repacker or flasher.

The read-only [`analyze_main_ram.py`](../h9-firmware-research/analyze_main_ram.py) script decodes a chosen range from that stock RAM-code slice at its RAM address after checking the pinned app hash. It keeps disassembly output in ignored `output/`; it does not embed vendor code in the replacement or identify an interrupt vector by itself.

`main/h9_main_serial_commands.*` now ports the next two stock RAM transactions at
`0x58` and `0xA0`: read and write commands through the byte-exchange routine.
`h9_main_serial_gate.*` ports their interrupt-protected gate/refcount pair,
and `h9_main_serial_port.*` binds the commands to that gate and exchange.
The target linker places these routines in relocated RAM and checks the
critical-section opcodes. Host tests cover word order, returned data,
reference counts, and cleanup after a failed exchange. No production board
entry invokes this port. See [the RAM serial trace](../h9-firmware-research/evidence/stock/main-ram-serial-bus.md).

```powershell
py -3 firmware-research/replacement-source/link_main_check.py --compiler 'C:\JL\pi32\bin\clang.exe' --stock-image 'C:\path\to\app.bin'
```

The Windows JieLi toolchain check linked all current main objects with no unresolved symbols. The diagnostic ELF cannot boot the radio: most RAM code is missing, and stock calls a large board initialization routine after its retained startup record. The diagnostic reset path processes that record in the observed order, calls the first two stock board ROM entries, initializes the observed allocation bitmap and its metadata, initializes the first memory window, writes two RAM guards and three early state bytes, runs the first four-channel clock measurement, register-selection, 240 MHz calibration-band selection, bounded arithmetic, source/divider search, timing calculation, first clock-apply transition, ROM mode setup, repeated selector, and the MMIO/source/latch/auxiliary sequence through the helper's return, then runs the first post-clock ROM block and hardware-field classifier. Board drivers and a complete Pi32 memory map remain unported. See [the stock startup trace](../h9-firmware-research/evidence/stock/main-startup.md), [the bitmap trace](../h9-firmware-research/evidence/stock/main-heap-bitmap.md), [the memory-window trace](../h9-firmware-research/evidence/stock/main-memory-window.md), [the board-marker trace](../h9-firmware-research/evidence/stock/main-board-markers.md), [the clock-probe trace](../h9-firmware-research/evidence/stock/main-clock-probe.md), [the timing trace](../h9-firmware-research/evidence/stock/main-clock-timing.md), [the apply trace](../h9-firmware-research/evidence/stock/main-clock-apply.md), [the ROM mode trace](../h9-firmware-research/evidence/stock/main-clock-rom-mode.md), and [the post-clock trace](../h9-firmware-research/evidence/stock/main-post-clock-board.md).

The stock clock source register writer and latch helper have injectable MMIO bindings and Pi32 instruction checks. The diagnostic board entry now calls them after the intervening MMIO field sequence beginning at app+`0x0C55AA`. The post-clock board path also reaches the next two-ROM handoff at app+`0x05FCD4`; its result words are captured for the still-unported GPIO setup.

The following auxiliary clock helper writes three fields of `0x1E0008`, calling ROM `0x02000110` between the first and second writes. Its ROM callback and register are injectable for host tests. The Pi32 link check verifies the emitted field order and ROM target. The diagnostic board entry calls it before the final `0x02001650` ROM transition.

The main APRS mute module now also has an audio-route transaction. It reads the live receive side and the ordinary speaker/Bluetooth requests under a board-supplied route reservation, overlays the burst lease, then applies both outputs. It refuses to act if either output setter is absent and never turns on an output that normal squelch routing requested off. A host integration test drives the replacement APRS activity frame through the main session and verifies both output requests, voice on the other side, and restoration after the clear. The physical Bluetooth output setter, verified live receive route, and board reservation are still unimplemented; this code alone cannot mute the radio.

## Next source work

1. Trace and implement the remaining command `0x51` side effects and UART board hooks; connect the one-shot `PY` ready event, parsed GPS, and packet processing to the UI and data model.
2. Validate the ported ADC-to-packet receive chain with recorded APRS audio, then map the board's carrier-sense and receive-event signals and connect the send gate to packet encoding, PTT, and DAC hardware.
3. Identify the main radio's APRS ADC tap and the two audible-output gates; connect the mute policy only after that signal path is established.
4. Map Pi32 RF preparation and the precise PTT control point, connect the TX service to a board adapter, then verify the actual emitted frequency into a dummy load.
5. Implement boot, storage, UI, Bluetooth, interrupts, and board initialization. Build both target images, validate update formats, and test recovery before a radio trial.

See [the firmware research map](../h9-firmware-research/APRS_CODE_MAP.md) for the observed offsets and limits of the current evidence.
