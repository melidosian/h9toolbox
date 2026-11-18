# Resident dispatch foundation (offline only)

This directory is deliberately not linked into the firmware or module bundle.
It implements a testable single-task cooperative dispatch/ownership contract,
not an RTOS service, stock RF lock, or authorization to transmit.

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

## Integration plan

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
rollover and two-sector settings power cuts. The placement probe links the
scheduler and settings engine for Pi32v2 and checks the budget of the erased
sector reported by the backup. It creates an offline-only ELF and JSON report
in `build/resident-probe`.
