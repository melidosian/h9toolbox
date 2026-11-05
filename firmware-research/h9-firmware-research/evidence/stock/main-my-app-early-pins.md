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
Each of the four low-level helpers calls the pin-bank lookup at app+`0x0000BA`.
Its table at app+`0x000554` contains `0x1E5000`, `0x1E5040`, `0x1E5080`,
and `0x1E50C0`, indexed by `pin >> 4`. For the five early `my_app` pins,
the helpers take the ordinary branch: app+`0x000176` with value 0 clears
bit `pin & 15` at bank offset `0x0C`; app+`0x0001F6` with value 1 sets
it at offset `0x08`; app+`0x000284` with value 0 clears it at offset
`0x10`; and app+`0x0002DC` with value 0 clears it at offset `0x14`.
The pinned helper slices app+`0x000176..0x0001D8`,
`0x0001F6..0x000266`, `0x000284..0x0002BE`, and
`0x0002DC..0x000316` hash respectively to SHA-256
`ff1d49dc53e7d4009adade553b6b174ec6e06dbc5f39ed22b0e15aeee0d921ee`,
`0f670e0955c90397866b2c1df4336d5a31bd3693b79b99fc1f621ce01a6f64b5`,
`d395d057d9c7e10d865a7f13356ed71d7c97840a35b121704c9377df4987d3a8`,
and `a189d9c21707e6741945d2cf271d87f7aac9df85d567d03ac7e26847ff04e03d`.

Pin 38 and pins 56 and above use different branches; the replacement
`main/h9_main_pin_actions.*` rejects them. `main/h9_main_pin_registry.*`
models the observed call and registry sequence. The registry's initial
contents, other pin branches, and the rest of `my_app` remain unported;
this does not make the task runnable.
