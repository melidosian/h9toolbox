# APRS update entry at power-on

The stock TD-H9 v1.0.38 main application was decoded at virtual base
`0x01E00100`. Its SHA-256 was
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The startup, UART setup, and update-frame handling ranges below also
matched the manifest-verified KISS main image.

| Main address | Observed action |
| --- | --- |
| `0x01E88538–0x01E8857A` | Opens the modem UART with a 19,200 baud request, saves the serial object at `0x11CFC`, and initializes the receive ring. |
| `0x01E8857E` | Calls the stock update screen at `0x01E7A052` before sending the normal modem startup message. |
| `0x01E7A064–0x01E7A078` | Checks two key-state bytes at `0xC39D` and `0xC3AF` for `14` and `9`. The observed power-on combo is PTT+7. A keypad value of `9` means digit 7. |
| `0x01E7A1A4–0x01E7A1C6` | Waits while servicing keys and timers. Back (key value `15`) returns from the screen. A valid modem `PY 0x55` sets `0x11B32`, which ends the wait and displays “Update Success!”. |
| `0x01E88588` | Invokes `0x01E7A1FE`, which sends `4A 4C 03 00 55 01 EF` through the stock formatter and modem UART. |
| `0x01E8858C–0x01E8859A` | Invokes UART object slot `+24` with `0x1C200` (115,200). The UART1 object assigns that slot to `0x01EC57E4`, which writes the divisor register at `0x001E2108`. The APRS application also initializes its UART to 115,200. |

The stock updater opens the internal link at 19,200 and waits in PTT+7
before performing the normal launch sequence and switching to 115,200.
The 38,400 setting in the desktop updater belongs to the host-facing
serial interface. These are configured rates. The board's waveforms and
the bootloader's internal state remain to be measured.

The host write path sends `JL 0x0D` with the four address bytes and the
block data, then reports a host ACK when the modem returns
`50 59 02 01 0D B9`. Host exit sends the zero-payload form
`4A 4C 02 00 55 ED` after replying to the host. The normal startup message
has the extra `01` payload byte. Both use the low eight bits of the sum
from the start of the frame. A modem reply of `50 59 02 01 55 01` is the
update screen's success signal. [Formatter and receive path](aprs-main-uart-ring.md)

## No-write hardware check

On 2026-11-18, the user disconnected USB, cold-started with PTT+7, then
pressed Back from APRS Update. APRS Info showed `V1.0.15` afterward.
This confirms that the stock startup pause and normal modem launch work
without a host connection. Writing a staged image from the radio remains
untested. A separate transition into the updater from an already-running
modem remains unidentified.

## Candidate on-radio path

Stage and verify the approved image before shutdown. For the first version,
use PTT+7 on cold boot and run the staged updater after the stock modem UART
initialization and before the stock launch call. Preserve the stock USB
update route for recovery. The stock application initializes the resource
SPI path later at `0x01E8859C–0x01E885C0`, so a staged writer that reads the
resource chip at this earlier point needs verified SPI setup.

Use the stock 19,200 baud phase and send one `JL 0x0D` block at a time.
Accept progress from the modem's own `PY 0x0D` replies. Wait for its exit
reply, let the normal startup sequence switch the link to 115,200, then
query the running APRS application with `0x20` and require a valid
`0x21` version reply before reporting a working modem. A version reply
proves the application is running. Stock and the KISS patch both report
`V1.0.15`, so a packet receive test is also needed to distinguish them.
