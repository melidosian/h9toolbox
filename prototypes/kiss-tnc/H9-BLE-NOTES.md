# TD-H9 BLE packet connection investigation

## Evidence and limits

The local `td-h9-open-firmware` project contains `docs/main-ble-and-timing.md`, `reverse-engineering/main/ble-interface-map.json`, and the qualified framing model `tools/inspection/main_ble_write.py`. These report static analysis of decoded main v1.0.38 app SHA-256 `11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`. Original firmware bytes were not changed or copied into this project.

The recovered table at app offset `0xB5C9C` identifies AF00 with AF01 write-without-response (handle `0x0017`) and AF02 notifications (handle `0x0019`, CCC `0x001A`). Write callback `0x5DBA8` dispatches to `0x5DCDA`, then framing routine `0x5DACC`, AX.25 parser `0x5D9F6`, and packet submission `0x5B3EA`. This is a static main-to-modem submission path, not RF validation.

The model has fresh framing state on each write. A complete first frame is processed; an incomplete frame is not retained for the next write. Leading consecutive delimiters terminate an empty first frame. This differs from the persistent generic KISS decoder used for incoming notifications.

The previous browser implementation wrote KISS data to FF02 on the programming service FF00 and split every frame into 20-byte writes. Both behaviors conflict with this recovered packet path. The corrected H9 connection requests only AF00 and submits one complete frame using AF01 write-without-response. No fallback to programming characteristics, chunking, initialization commands or automatic retries are used for H9.

## Software reproduction

Using the independent local Python model, the browser encoder produced a 30-byte frame for `N0CALL>APRS:>bench test`. One write parsed as complete, command 00, with the expected AX.25 payload. Splitting that same frame into 20-byte pieces produced two incomplete results. The model's six existing tests also passed. This checks framing only; it does not emulate the CPU or establish RF/PTT behavior.

Repository tests mock passive AF00 discovery, programming-only rejection, whole-frame TX, write-error propagation, port-0 selection, generic transport behavior and cleanup. Run `node --test test/*.test.mjs`.

## Next hardware check

1. Disconnect and reload the local page so the old programming connection is released and the new code loads.
2. With the existing dummy-load setup, enable APRS and select the intended APRS TX channel on the radio. Leave KISS port at 0 and use your own callsign.
3. Click Connect Bluetooth and select the TD-H9. The status must say APRS packet interface (AF00). An absent service is reported rather than silently using the programmer.
4. Enable transmission and manually send a short packet once. Record the exact browser result and the radio's TX indication. No automatic transmission is performed by the app.
5. If the write fails, record its full error: the browser cannot configure ATT MTU here. If it succeeds but RF remains idle, capture ATT write boundaries/length and the radio's firmware/APRS settings before adding any new radio commands.

Actual service discovery, ATT write boundaries, radio state, PTT and RF output remain hardware validation items. Host tests alone do not make this a validated transmitter.
