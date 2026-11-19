# Resident SmartBeacon bench build

The `--resident-beacon --aprs-mute --disable-morse --dev-kiss` build adds an
ABI 4 APRS BEACON screen and a scheduler task independent of the stock radio
and Toolbox screens. It produces a separate body for main flash sector
`0xFB000`. Startup verifies its CRC before entering it. ABI 3 builds continue
to use the legacy modules. The ABI 4 loader requires the CRC of its matching
APRS BEACON screen and rejects the old background module.

The worker polls the UART1 DMA descriptor every two 100 Hz ticks. It takes
bounded copies of published bytes with its own cursor, leaves the stock
consumer unchanged and clears partial GPS input after a buffer gap. Live
status and settings are shared with the screen through versioned snapshots.
The worker restores arm, mode, interval and symbol from the committed
settings record. It records due events with live GPS input. Automatic
transmit is disabled in this bench build.

The screen saves settings to the two guarded resource sectors after checking
the GAME directory. The journal retains a prior committed record across an
interrupted save. A degraded journal restores the verified mute preference
with automatic scheduling disarmed. The stock speaker gate uses the
verified preference. See [normal-mode APRS mute](../APRS-MUTE.md).

## Automatic TX dev build

Build with `--aprs-mute --disable-morse --dev-kiss --resident-beacon
--flash-shared --resident-tx`. Use the resulting main package, its matching
`h9_modules.bin` and the verified resident ABI 2 body
`resident_0xFA000_OFFLINE.bin`. Reserve main-flash sectors `0xFA000` and
`0xFB000` for that body. Back up both sectors before installation and verify
the updater can restore them. Earlier experiments may have left data in them.
The main package leaves that area out of its write range. The startup CRC
must match the separate body before automatic TX can run. The loader stays
on API 5, and the APRS modem and settings journal use their existing formats.

In the stock APRS menu, set APRS Switch on, choose the TX channel and set
the call sign and SSID. Leave the stock timed beacon, PTT Linkage, DIGI1
and DIGI2 off when using the Toolbox schedule. In **Toolbox → APRS SETTINGS**,
choose Smart or Timed and arm the schedule. The saved schedule includes its
APRS selector and frequency. Disarm and rearm after changing either. The
existing APRS beacon setup accepts 2 m frequencies.

The GPS worker follows the schedule across screens and restores it on
reboot. The radio sends pending beacons from its normal channel screen.
The first beacon becomes eligible after the 15-second guard and a fresh
GPS fix. Timed mode measures the next interval from the completed send.
Smart mode uses the existing speed, turn and stop rules. A Lock selection
waits for its VFO to be selected. Auto can briefly borrow the APRS VFO,
check for a carrier and restore the previous receive route after TX.
When a packet is handed to the modem and TX ends, **APRS → BEACON STATUS**
increments **TX SUBMITTED**. Use an independent decoder to confirm reception.

### H9 bench checks

1. Save the main firmware identity, both resident sectors, the installed
   app bundle and the SmartBeacon settings. Use PTT + 2 recovery for main
   flash, keep the supported APRS modem, and confirm the installed files
   against the build manifest.
2. Connect a dummy load or an attenuated test setup before the first normal
   boot. An already-armed schedule resumes on boot. Use different A and B
   frequencies to distinguish which side transmits.
3. With B Auto and A selected, confirm a timed beacon on B and another
   after the chosen interval. Decode the AX.25 frame with an independent
   receiver. Repeat in Smart mode after arming and getting GPS, then check
   that the saved schedule sends again after a reboot.
4. Confirm the radio returns to the selected voice channel. Exercise busy
   stock receive, a carrier on the borrowed APRS VFO and Lock with the
   other VFO selected. Check that a blocked beacon stays pending and can
   send once the route is available.
5. With APRS Mute on, verify audible voice on the voice VFO and receive a
   known APRS packet in the stock Beacon List without hearing the burst.

## Cooperative dispatch reference

`dispatch.c` remains a host-tested single-task ownership model. The dedicated
worker does not link it or access RF state.

`dispatch.c` provides:
- exact ABI/descriptor-size checks, magic checks and required caller/provider
  capabilities (cooperative context, permanent residency, audited integration);
- recursion rejection before the callback runs again;
- nested suppression, with sticky faults on underflow/overflow;
- exclusive modeled module/service ownership with generation-tagged leases;
- sticky faults for stale/wrong releases, generation exhaustion, and a callback
  returning with ownership or suppression unbalanced. Faults do not unlock RF.

Capabilities are trusted integration declarations, not a security boundary.
The binding pointer must already be known readable, aligned, and resident.
Header validation cannot validate arbitrary addresses or verify code integrity.
State initialization is boot-only after RF quiescence, never a way to clear a
fault while a radio operation is in progress. Do not retain this structure across
boots without a separate versioned/integrity-checked persistence contract.
A callback must return normally; no longjmp, exceptions, or asynchronous use.
All calls must execute in the SAME task, outside ISR/critical sections. No atomics
are supplied; this cannot serialize stock tasks that do not participate.

## Earlier cooperative integration plan

1. Link scheduler and this dispatcher into permanent far-code, not the 8 KiB
   module overlay. Verify package/version/CRC before publishing its binding.
   Reserve state RAM by linker/map proof, with no module scratch overlap.
2. Extend a versioned loader API rather than hardcoding an unvalidated callback
   address. Old modules receive no advertised audited capability.
3. Add recursion-safe calls in rebuilt `h9_key`/`h9_delay` wrappers. Preserve their
   ABI and return values. Only audited module call sites may advertise capability;
   a raw wrapper call from an ISR or RF critical section is not eligible.
4. Audit every module's longest non-yielding path. Bound long delays with service
   opportunities if safe. This yields coverage for audited modules, not arbitrary
   modules, blocking stock menus, or modules that never call these wrappers.
5. Use suppression around display-critical/radio-critical operations and leases
   for cooperative module radio use. An unmatched release latches a fault; stop
   service and report it instead of pretending the radio is free.
6. Initially callbacks may only update scheduler/status/pending state. There is
   NO transmitter callback here. Before actual TX, prove stock task arbitration,
   busy/side/frequency/PTT safety, pending/retry semantics, and safe restore across
   blocking KISS calls. Modeled ownership alone does not meet this requirement.
7. Bench-test task identity, stack margin, callback latency, reentry, all menu
   transitions and recovery before enabling any on-radio transmission.

## Verification

From `firmware-research/module-loader` after the normal module build:

```sh
python3 -I test_modules.py --native
python3 -I resident-service/build_probe.py --far-backup <private-far-backup> --toolchain <toolchain>
```

The host tests exercise shared status, pending IDs, GPS expiry, retry, clock
and DMA wrap, input gaps and two-sector settings power cuts. The simulated
radio also covers A/B selection, carrier checks, frame production,
asynchronous stock I/O and restoring receive after transmit. The placement
probe links the scheduler and settings engine for Pi32v2 and checks the budget
of the erased sector reported by the backup. It creates an offline-only ELF
and JSON report in `build/resident-probe`.
