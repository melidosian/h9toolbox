# First `my_app` pin setup calls

Source: stock TD-H9 v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
Addresses use the inferred `0x01E00100` application base. The independent
Pi32v2 r3 instruction decoder was used on the exact image bytes.

At app+`0x088180`, `my_app` reserves 124 bytes and calls app+`0x0515C6`
with `r0=500`. It then calls app+`0x079BE4` five times with `(r0,r1)`
equal to `(12,4)`, `(37,13)`, `(36,10)`, `(10,3)`, and `(1,0)`.
The caller slice app+`0x088180..0x0881B8` hashes to SHA-256
`be88e1ea40473b58689bd49147ad60491bac3cf8d80e335a573414e74dd7fde3`.
The 500-unit delay's actual duration remains unverified.

The helper at app+`0x079BE4..0x079C2E` hashes to SHA-256
`2c56fb85c54aebe0b9ecf7c442859e804369bf2cae285c5dd78e950842111cf3`.
It calls app+`0x000176` with `(pin,0)`, app+`0x0001F6` with `(pin,1)`,
app+`0x000284` with `(pin,0)`, and app+`0x0002DC` with `(pin,0)`.
It then scans ten eight-byte RAM records beginning at `0x12A30`. The first
word is a signed key. An equal key returns without changing the record;
the first `-1` key is replaced and the halfword at record offset 4 becomes
`1`. If no matching or empty record exists, it returns without insertion.
Those four low-level helper effects and the registry's initialization path
remain unported. `main/h9_main_pin_registry.*` models the observed call
and registry sequence with injectable low-level actions; it does not make
the full `my_app` task runnable.
