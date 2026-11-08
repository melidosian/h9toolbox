# TD-H9 resource service page

A local Web Bluetooth page for the [S1/S2 resource-flash service](../../firmware-research/resource-service/README.md).
Sections 1–3 read the radio's 16 MiB SPI resource flash in 32-byte blocks
using only the stock programming handshake, `R`, and `E` commands. Section 4
writes, and only works on an S2 radio. The page sends no PTT or APRS commands.

Run `python3 prototypes/resource-reader/start.py` from the repository root
and open the printed localhost URL in desktop Chrome or Edge. Put the radio
in its normal Bluetooth programming mode and disconnect other radio apps.

1. **Check service** reads status slot 0: service version, the stock
   flash-ready marker, and the chip's JEDEC ID. A radio without the service
   image fails this check without side effects.
2. **Read** a partition, the whole chip, or a custom hex range. With
   *Read twice and compare*, the page reads the range twice and reports
   whether the SHA-256 hashes match. Only use matching dumps as backups.
3. Download the `.bin` and its manifest together.

   *Fast Bluetooth* (on by default) uses the image's bulk read. The radio
   streams up to 2 KiB per request as tagged notifications, and the page
   checks every byte against the radio's reported start, count, and cursor.
   It picks the largest packet size the link accepts. It needs a 32 KiB
   aligned start and falls back to 32-byte reads on an image without bulk
   support. The manifest records the method, packet size, and the TX
   capacities the radio reported.
4. **Run sector test** (S2 only) on a fully erased voice, font, or resource
   sector. A dump of a writable region suggests one. The page checks that the
   sector is erased, programs a test pattern block by block while checking
   the radio's status after each block, reads it back, erases it with the
   pattern's CRC, and confirms it reads `0xFF` again. Writes are never
   retried automatically. **Erase sector** is the recovery for a test that
   stopped part-way. It reads the sector first, and the radio refuses the
   erase unless that CRC still matches.

On a read timeout, the page leaves and re-enters programming mode, reselects the
bank, and resumes, up to five retries per step. The address map is in
`dist/boot-logo/resource.mjs` (shared with the website boot-logo page) and covered by `test/resource-reader.test.mjs`.
