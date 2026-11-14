# 06: top end of the chassis, looking down at the antenna connector and patch antenna

**Area:** top end of the radio with the case removed, hand-held, viewed from above.
Left: antenna connector. Centre: the small antenna board. Right: volume/power shaft.
The front face of the main board is at the bottom of the frame, seen edge-on.
Received 2026-11-13. Visual inspection only.

## Components

| Item | Reading | Confidence |
|------|---------|------------|
| Tan square block with a silver square on top, one solder dot off-centre, small notches cut in the silver edge | **ceramic patch antenna**. Single off-centre feed pin and trimmed edges are the standard build of a GPS/GNSS L1 patch. Size roughly 12–15 mm square (judged against the antenna connector) | part type read; GPS use inferred from the construction |
| Small green board under the patch, black coax leaving it | carrier board for the patch; the coax is the one that plugs into the U.FL socket on the front face (photos 01–03) | read |
| Piece of black cloth tape, half lifted | covered the patch (cushion or insulation against the case top); now peeled back | read |
| Antenna connector: threaded barrel with a centre pin, in a teardrop flange held by two cross-head screws | main VHF/UHF antenna connector, bolted to the chassis and not only soldered to the board, so antenna torque goes into the metal frame | read |
| Threaded bushing with a plain round shaft | volume/power control (rotary) | read |
| Small metal tactile switch with a round plunger, pointing up from the board edge | top button switch (the red silicone button in the case sits over it) | read; matches photo 01 |
| Cast metal chassis top wall with a pocket for the patch antenna | the patch sits in a cut-out so it faces the sky through plastic only; the metal around and below it acts as its ground plane | inferred |
| Gold pins and gold spots at the bottom of the frame | parts on the front face seen edge-on, out of focus | not identified |

## Corrections to earlier notes

- Photos 01–05 described the tan block as "ceramic antenna element or foam pad".
  This photo settles it: it is a ceramic patch antenna. The GPS reading moves from
  "unverified" to "inferred from construction". It is still not confirmed by a
  marking or by firmware.

## Diagnostic notes

- The patch points straight up when the radio is held upright, which is the correct
  orientation for satellite reception.
- The patch is passive at this point: no amplifier is visible on the carrier board
  from this angle. Any low-noise amplifier would be at the U.FL end or on the back.
- The GPS receiver chip itself has not been seen yet.
