# 03: whole front face of the board, LCD removed, with front case and LCD

**Area:** complete front face of the main PCB in its metal chassis (radio lying on
its side, top of the radio to the right). Above it the inside of the front case;
at right the removed LCD and the volume knob. Received 2026-11-13. Visual only.

## Main board, front face

| Item | Where (radio upright) | Reading | Confidence |
|------|----------------------|---------|------------|
| 19 metal dome keys under clear tape | lower half: 4×4 block, plus a column of 3 beside the speaker | keypad. Domes short gold ring pads on the board | read |
| White SMD LEDs between the keys, about 7 | keypad area | keypad backlight | read (count approximate) |
| Row of 5 small empty plated holes | bottom left corner of the keypad area | **unpopulated header, likely a factory programming/debug port** | holes read, purpose inferred |
| Two gold coil springs inside a silk circle | centre | speaker contacts; the circle marks the speaker position | inferred |
| Small trimmer with a cross slot | lower left of the speaker circle | adjustable part: trimmer resistor or trimmer capacitor (factory alignment) | read; type unverified |
| Column of about 6 small parts | below the trimmer | possibly the keypad resistor ladder (firmware reads the keypad as two ADC ladders, channels 13 and 10) | unverified |
| Round metal can with dark felt top | bottom right of the speaker area | electret microphone | read |
| Light grey printed circle and a two-pad part with `+ -` silk | beside the microphone | blank label spot; polarised two-pad part not identified | unverified |
| U.FL socket with coax | right of the speaker circle | feed to the top-edge antenna board (photo 01) | read |
| Gold metal bar, one screw | between keypad/speaker area and display area | see photo 01 | unverified |
| FPC connector, `BAT` pad, `R200` group, air coil, bare gold pad | display area | see photo 01 | |
| Two small plated pins | bottom edge | possibly charge/dock contacts | unverified |
| Side-mounted switch tabs | long edge opposite the jacks | PTT and side keys | inferred |
| Two accessory jack barrels | right edge near the top | speaker/mic jacks (the loose rubber cover in photo 05 has headphone and microphone icons) | inferred |
| Cast metal chassis with screw lugs and battery-latch tabs | around the board | heat sink and structural frame | read |

## Front case (inside)

- Speaker glued into the case, round, about 36 mm. Label text is not legible in this
  photo. Two tan solder pads on its back meet the two coil springs: there are no
  speaker wires.
- One-piece translucent silicone keypad mat (16 + 3 keys).
- Red silicone piece at the top: seal/actuator for the top button.
- Hole at the top for the volume shaft.

## LCD module (right)

- Small colour TFT in a lime-green plastic carrier, orange flex tail. Firmware drives
  it as 160×128 (`resource-flash-log.md`, C-hello entry). No part number visible
  from the front. The flex is the place to look for one.

## Diagnostic notes

- The 5-hole footprint is the most useful find for firmware work: it should be
  probed (power off: continuity to ground and to the `BAT` pad; power on: voltages,
  then a logic analyser) to see whether it carries UART or the chip vendor's
  programming lines. Nothing has been probed yet.
- The speaker connects by spring pressure only, so the board can be run on the bench
  out of the case with no speaker attached.
