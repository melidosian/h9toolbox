# TX context evidence and non-transmitting observation plan

The resident bench build runs GPS and scheduler work in a dedicated task.
Its pending event can be read by Toolbox while the stock radio and other
screens continue. The stock RF paths retain their current call sites.

## Evidence, not inferred mutual exclusion

Stock CPU addresses, v1.0.38:

- `0x01E5DBCC(buf,len)` synchronously deframes KISS, decodes AX.25,
  selects logical side `0x11B1D = 0x11B50 >> 7`, calls PTT handler
  `0x01E5B144`, sends via `0x01E5B4EA`, waits airtime via `0x01E5C36C`
  (fallback 500 ms), and calls end-TX `0x01E5CDEC(0)`.
- End-TX calls `0x01E58F58(0)`, `0x01E5CCCA`, clears TX state and
  restores receive via `0x01E595B6`.
- Stock beacon `0x01E5C3D4` explicitly sets TX flag `0x11D28`, keys RF
  and sends; its caller performs end-TX. This flag is observable state,
  not a demonstrated atomic acquire operation.
- The 2026-11-10 APRS SEND hardware report proves a synchronous module
  invocation can key/send/unkey. It also reports stock TX UI appearing and
  initial wrong-side transmission until side setup was added.

Sources: `../../resource-flash-log.md`, sections “Stock APRS transmit paths”
 and “First H9 Toolbox APRS transmission”; `../../TD-H9-HANDBOOK.md`.
The source paths are relative to this directory.

There is no demonstrated mutex/queue contract covering KISS, stock beacon,
physical PTT, dual watch and other module RF operations. A busy check followed
by a call is not atomic exclusion. Reusing a task context does not prove that
other tasks/interrupts cannot modify RF state during the blocking call.

The stock timed caller at app+`0x87494` is reached by `my_app`. It calls
app+`0x82EB2` under the stock mode and activity checks. This is a possible
consumer of the resident pending event. A direct scan also found the
manufacturer command handlers reaching the PTT wrapper at app+`0x5DD0A`
and app+`0x5E1BC`. The exact concurrency of those handlers still needs
validation before a pending event can enter the stock transmit path.

## Concrete safe integration boundary

A permanent scheduler-only callback at audited module `h9_key`/`h9_delay`
call sites, on the existing my_app task, is suitable for parsing snapshots,
computing due/pending status and recording diagnostics. It must not call KISS,
PTT, retune, quiet audio, or restore side. The dispatcher recursion guard allows
nested wrappers to return without rescheduling. Each RF-using module must
participate in suppression/ownership before any future transmit callback is
considered. Existing modules that do not cooperate remain unsupported.

## Non-destructive bench plan (not performed)

1. Record exact firmware/module hashes and settings backup; verify USB recovery
   remains available. This task does not authorize flashing diagnostic changes.
   Use existing read-only RAM/serial diagnostics if available. If timestamped
   execution counters require new instrumentation, prepare a separate reviewed
   no-TX build and obtain authorization to install it before proceeding.
2. Do not press PTT, arm APRS, trigger manual beacon, or alter channels/settings
   for this observation. Disable any already-running transmission at the user's
   controls before the session. If that cannot be assured, stop the test.
3. Observe monotonic tick `0x1D8B0`, stock TX flag `0x11D28`, logical side
   `0x11B1D`, side selection `0x11B50`, and working frequency `0x11C8C`.
   These addresses come from existing traces; they are read-only observations,
   not write targets or guarantees of a coherent multiword snapshot.
4. In diagnostic-only code, record callback entry/exit counters, maximum gap,
   maximum duration, recursion skips, suppression depth, owner and stack high
   water if a verified measurement exists. Do not introduce a guessed task-ID
   ROM call. Establish context from the known synchronous loader call chain.
5. Visit stock channel screen, Toolbox menu, each audited module and its nested
   menus; reopen APRS screens; exercise ordinary key holds, redraws and waits.
   Log gap distributions and prove pending/status survives overlay changes.
   Fail on callbacks in disallowed contexts, nonzero leaked ownership, state
   corruption, display regressions, stalled keys, or unbounded callback gaps.
6. While RECEIVE-only, observe existing incoming traffic/dual-watch behavior
   and verify suppression never changes the radio. Do not induce RF transmission
   merely to test busy detection. Busy-path transitions are covered offline.
7. Stop on any unexpected TX indication. Preserve logs and revert only through
   the established recovery procedure if needed; no automatic memory repair.

Passing these observations validates cooperative liveness and noninterference,
NOT TX arbitration. A later explicitly authorized RF bench phase needs a verified
stock serialization mechanism (or complete relevant producer audit), suitable
RF containment, exact TX/abort/restore assertions and failure recovery. This
checkpoint deliberately enables no transmit path.

## Placement experiment

`build_probe.py` links the scheduler, dispatcher and settings journal at
candidate CPU address `0x01EF6000` for main-flash sector `0xFB000`. That sector
was erased in the November 17 readback. The 17 earlier sectors hold data.
The 3,308-byte link leaves 788 bytes in the candidate sector. The board
adapter, loader validation, callback context and recovery coverage still
need verification.

## Modem input during Toolbox execution

The [stock UART trace](../../h9-firmware-research/evidence/stock/aprs-main-uart-ring.md)
shows that the writer drops a whole batch when the stock read cursor leaves
insufficient room. A second read cursor cannot make room for the stock
writer. The stock frame consumer in `my_app` also stops advancing while a
Toolbox module owns that task. Continuous Beacon input requires either the
stock consumer to keep running or a bounded copy ahead of the stock ring's
overflow check. Keep parsing and scheduling out of the UART interrupt.

The [JieLi AC63N timer guide](https://test03.jieliapp.com/PublicDoc/AC63N/html/DST/%E7%AC%AC%208%20%E7%AB%A0%20%E7%B3%BB%E7%BB%9F%E5%AE%9A%E6%97%B6%E5%99%A8%E6%8E%A5%E5%8F%A3%E8%AE%BE%E8%AE%A1%E8%AF%B4%E6%98%8E.html)
describes ordinary software timers posting callbacks back to the registering
task and user timers running in a hardware timer context. This suggests
verifying a separate task on BR23 for passive polling. The SDK guide covers
AC63N and supplies no validated BR23 callback address or RF lock.
