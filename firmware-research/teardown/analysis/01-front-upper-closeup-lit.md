# 01: front side, upper half, LCD lifted (close-up, brightly lit)

**Area:** front (keypad/display) face of the main PCB, upper half, the part normally
hidden under the LCD. Antenna connector top left, volume/power shaft top right.
Received 2026-11-13. Visual inspection only.

**Board:** single main board, blue solder mask, screwed into a die-cast metal chassis.
This face carries the user-interface parts and some supply/RF passives; no large ICs
are on this face in this area (the MCU, RF chips and memory must be on the back).

## Components

| Item | Where | Reading | Confidence |
|------|-------|---------|------------|
| Antenna connector, threaded, in a cast metal block | top left | SMA-type | read |
| Right-angle tactile switch, 3 solder tabs | top edge, right of antenna | top button switch (the front case has a red silicone button above it) | inferred |
| Small green daughterboard standing at the top edge, tan block on top, black coax soldered to it | top centre | separate antenna board fed by coax; the tan block is a ceramic patch antenna (confirmed by photo 06), most likely GPS | inferred |
| Black micro-coax with gold U.FL/IPEX plug | from lower right up to the antenna board | feed for that antenna; the receiver it serves is near the U.FL socket or on the back | inferred |
| Two SMD LEDs side by side | top edge, right of the antenna board | TX/RX status LED pair under the top light pipe | inferred |
| Through-hole solder joints in groups | top right and right edge | volume/power control and the two accessory jacks (their barrels show at the right edge) | inferred |
| Small bent metal spring clip | upper right, top of the white strip | ground/shield contact | unverified |
| Resistor marked `332` (3.3 kΩ), large package | upper left by the screw | | read |
| Bare gold rectangle, no part | upper centre | exposed ground/thermal pad; contacts the LCD's metal back or spreads heat from a part on the back | unverified |
| Flip-lock FPC connector, about 20 contacts, fine pitch | left centre | LCD flex connector. A column of about 10 small parts next to it sits in the signal lines | read (pin count not counted exactly) |
| SOT-23 marked `6C` | below the FPC connector | `6C` is the usual code for a BC817-40 NPN transistor; position suggests the LCD backlight switch | marking read, role inferred |
| Silk label `BAT` with a round gold test pad | centre | battery-voltage test point | read |
| Empty two-pad footprint (tinned) right of `BAT` | centre | unpopulated part, diode or fuse sized | read |
| Five resistors marked `R200` (0.200 Ω), large power package | centre, in a column | low-value power resistors in a high-current path. Whether they are in series or parallel cannot be told from the photo | marking read, role unverified |
| Three unmarked chip parts left of the R200s (two dark grey, one tan), two grey ones to the right | centre | ferrite beads/inductors (grey) and a capacitor (tan) | inferred |
| Air-core wire-wound coil, about 10 turns, in its own silk box | right of the R200s | RF inductor (filter or choke), so RF or PA supply current flows on this face | inferred |
| SOT part marked `717` plus two more small SOT parts | right, beside the white strip | not identified | marking read |
| White vertical strip | right | silkscreen bar, no component | read |
| Gold-coloured metal bar held by one screw, two embossed windows | bottom, just below the LCD position | clamp or ground strap; may press a hot part on the back against the chassis | unverified |
| Gold coil spring | bottom centre, inside a silk circle | one of two speaker contacts (see photo 03) | inferred |
| White SMD LED | bottom left | keypad backlight | inferred |
| Solder tabs along the left board edge | left | side-mounted switches (PTT and side keys) | inferred |
| Silk text `393856A260509P18411423` | bottom | PCB fabrication/lot code | read |

## Diagnostic notes

- `BAT` test pad: a place to measure battery voltage on the bench. Firmware reads
  battery voltage on ADC channel 4 (`evidence/stock`, battery note); this pad is the
  likely analogue source to compare against. Not yet measured.
- The five 0.2 Ω resistors and the air coil suggest the transmit supply path runs
  under the display. Needs the back-side photos to confirm.
- No IC part numbers are visible on this face in this area.
