# APRS controller TX-gate state pointers

Source: captured APRS v1.0.15 image, SHA-256 `1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33`, in the `H9Aprs` Ghidra project. These are static observations, not a live radio trace.

`FUN_08005760` reads a pointer from RAM `0x20000100`, then tests bytes at offsets `+4` and `+3` and a word at offset `+0xE0`. It uses byte `+4` to choose between a random persistence draw and receive-frame processing. The literal at `0x080057F0` identifies the pointer slot. `FUN_08005800` writes its first argument into that slot; the call from `FUN_080056EC` passes the literal `0x20000230` at `0x08005754`. `FUN_080051AC` initializes that `0x20000230` object as a `0x330`-byte frame state, and `FUN_080051FC` stores received frame bytes beginning at offset zero. `FUN_08005180`/`FUN_08005830` forward a completed frame from the same object.

The gate's `+3` and `+4` bytes therefore overlap the receive frame byte array in this static layout. Their electrical meaning, if any, has not been established. The replacement `H9AprsTxGate` takes abstract busy/receive inputs and has no board binding; it must not be connected to a presumed carrier-sense GPIO based only on these offsets. The status/event word at `+0xE0` also lies inside that object and needs a separate write-site trace before use.
