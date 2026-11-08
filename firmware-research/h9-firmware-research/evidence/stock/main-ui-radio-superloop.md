# `my_app` superloop: UI and radio are interleaved

Source: stock TD-H9 v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`, base
`0x01E00100`. Linear Pi32v2 decode with `analyze_app.py disasm`; call targets
are `site + instruction length + displacement` (2 for short calls, 4 for long).
Static evidence only.

## Finding

Stock `my_app` (entry app+`0x088180`) is not just the UI. Its main loop,
starting at the call Brick Breaker hooks (app+`0x088906`), calls UI and radio
routines in one pass:

| Site (app+) | Target | Role (evidence) |
|---|---|---|
| `0x088906`, `0x088C30` | `0x01E58EE4` | Countdown at RAM `0x11AC4`; also called inside the RX/channel routine `0x01E52202` |
| `0x08890A`, `0x088982`, `0x088A8E`, `0x088C34` | `0x01E525A0` | Key handler: calls key decoder `0x01E515AC` and screen wake `0x01E5050A` |
| `0x08890E`, `0x088986`, `0x088A92`, `0x088C38` | `0x01E819F0` | Mixed: draw helper `0x01E50BBE` **and** RF output bit routines `0x01E50804`/`0x01E50820` |
| `0x088912`, `0x088C3C` | `0x01E8752E` | Calls `0x01E525A0`, `0x01E819F0`, and further UI/state helpers |
| `0x088916`, `0x088C54` | `0x01E7BB28` | Also calls the key decoder `0x01E515AC` and draw helper `0x01E50BBE` |
| `0x08891A` | `0x01E88064` | Standby tick (breathing-LED timer, see `main-visible-boot-signal.md`) |
| `0x088A2C`, `0x088A36` | `0x01E568C2`, `0x01E569D2` | Channel-to-RF copy (`rf-channel-copy.txt`) |
| `0x088A70` | `0x01E5AD68` | TX setup (`main-tx-frequency-precheck.md`) |
| `0x088BE4` | `0x01E5046A` | GPIO 9 LED writer |
| `0x088C02`, `0x088C9A` | `0x01E5234C` | Audio source request (`APRS_CODE_MAP.md`) |

The TX setup itself (`0x01E5AD68`) also calls the screen-wake routine and,
through `0x01E52C90`, the key handler. There is therefore no function-level
boundary between "UI" and "radio" in stock: a replacement UI must keep the
radio-side calls of this loop running, intercept drawing at a choke point
(window writer `0x01E4EDDE` / transfer submit), and act through stock routines
or shared RAM state.

## Consequences for earlier experiments

- **Snake** replaces the `my_app` task entry. Stock `my_app` never runs, so
  neither this loop nor its startup (which also initializes the resource
  flash, see `main-resource-flash.md`) executes. Radio behavior under Snake
  was not tested.
- **Brick Breaker** keeps stock `my_app`, but once launched it unwinds the
  `my_app` frame and loops in the game, so this loop stops. `app_core` and
  other tasks continue, which is why its README says not to touch PTT.

## Limits

- `pi32-call-index-v2.json` does not contain the `my_app` call sites above;
  it was built from a seed set that missed this body. Caller searches based
  on it are incomplete.
- The roles in the table come from callees and RAM use, not from behavior on
  hardware.

## Free code space in the stock app

Erased (`0xFF`) runs of at least 256 bytes in `app.bin`: 1,704 bytes at
app+`0xA77F0`, 1,004 at app+`0xA9D9E`, 857 at app+`0xA51A8`; one zero run of
1,036 bytes at app+`0xD30E4`. The app ends at flash `0xD8D2C`; the settings
area starts at `0xDA000` (4,820 bytes later, but sector `0xD9000` holds
package metadata). The repacker requires the app to keep its size. A larger
UI therefore needs reclaimed stock code or assets moved to the resource flash.
