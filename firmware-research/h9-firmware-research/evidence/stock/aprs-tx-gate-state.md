# APRS controller TX-gate state and command-0 dispatch

Source: captured APRS v1.0.15 image, SHA-256 `1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33`, analyzed read-only in the `H9Aprs` Ghidra project and checked against the raw Thumb bytes. These are static observations, not an RF capture.

## Two adjacent objects, not one frame buffer

`0x080056EC` calls `0x08002604(0x2000014C)` to clear and initialize the `0xE4`-byte demodulator/waveform object. It then calls `0x080051AC(0x20000230, 0x2000014C, 0, 0x08005181)` to initialize the adjacent `0x330`-byte receive-frame object. The former ends at `0x2000022F`; the latter begins at `0x20000230`.

`0x08005800` assigns its first argument (`0x20000230`) to pointer slot `0x200000FC`, its second argument (`0x2000014C`) to slot `0x20000100`, and its third argument (`0x200000D0`) to slot `0x20000104`. The literal words at `0x08005820`, `0x08005828`, and `0x08005824`, respectively, verify those destinations. Earlier notes reversed the first two slots and incorrectly placed the gate's state bytes inside the receive-frame buffer.

## Serial command 0 reaches the gate

The USART1 batch processor `0x08005F98` feeds bytes to `0x08005B8C`. On the closing `0xC0` delimiter, command `0x00` takes the compact Thumb jump table at `0x08005BAC..0x08005BBB`. The helper `0x0800501C` treats byte `0x05` at `0x08005BB2` as the entry count. For command zero it reads table byte `0x0D` at `0x08005BB3` and branches to Thumb address `0x08005BCD` (instruction at `0x08005BCC`). The code there loads the frame-object pointer from slot `0x200000FC`, the serial payload at `0x200005EC`, and its length from `0x20000108`, then calls `0x08005760` at instruction `0x08005BD6`. This explains why Ghidra's ordinary reference list showed no direct caller of `0x08005760`: it treated the inline table as instructions and omitted the branch target from the function body.

## What the gate observes

`0x08005760` reads its state pointer from `0x20000100`, so its `+3`, `+4`, and `+0xE0` tests address the **demodulator/waveform object** at `0x2000014C`, not the frame object. The independent bitstream routine `0x080055C4` writes `+3` for in-frame state and sets `+4` after at least six consecutive `0x7E` flags. It clears both on abort/overflow conditions. The ADC demodulator `0x08002308` clears `+4` after more than `0x60` samples without a transition. It sets bit 0 of word `+0xE0` when the bitstream routine reports a queue error, and may clear it after resetting the queue. These are software demodulator states. No carrier-sense GPIO has been identified.

When `+4` is zero, `0x08005760` repeatedly draws its LCG value at `0x0800211C` until the low byte is below the persistence setting at `0x2000011C`, then calls the encoder at `0x0800534C`. When `+4` is nonzero, it processes receive bytes through `0x080051FC` while `+3` is nonzero; a nonzero `+0xE0` makes it clear that word and exit without encoding. The stock body has no explicit tick delay and remains a blocking loop. Its scheduling interaction with the ADC ISR and main USART1 loop has not been measured.

The replacement exposes the demodulator's in-frame and preamble bits as an atomic snapshot for its cooperative gate, and uses its receive-queue overflow generation as the nearest observed error event. That event is an approximation of the stock `+0xE0` timing. The diagnostic board entry still rejects TX because the main radio's RF route authorization and PTT contract are unverified. This trace does not justify using the demodulator preamble bit as a general RF carrier detector.

Read-only Ghidra exports are stored privately under ignored `output/` as `aprs-gate-trace-20261104.txt`, `aprs-command00-trace-20261104.txt`, `aprs-frames-init-20261104.txt`, `aprs-tx-state-writers-20261104.txt`, and `aprs-demod-gate-state-20261104.txt`.
