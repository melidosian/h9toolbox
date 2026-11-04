# Timed beacon caller gate in main v1.0.38

The exact stock app has SHA-256 `11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`. At app+`0x08747C..0x087498`, virtual `0x01E8757C..0x01E87598`, its periodic caller invokes app+`0x082EB2` only if:

- byte `RAM[0x11AB0] <= 1`;
- byte `RAM[0x11AC6] == 3`;
- byte `RAM[0x11AFB] == 0` and byte `RAM[0x11AFE] == 0`.

The branch and call bytes are `00 FC 0E 02 81 F8 0C 06 50 EE 7B 02 50 EE 7E 12 10 19 61 FF FF 00 18 00 BF EA 0D DD`. Both the independent Pi32 decode in `aprs-timed-caller.asm` and targeted Ghidra decompilation show this gate. When it fails, the timer routine is not called and its counter at `0x11CEC` retains its value. The four bytes' semantic meanings and the caller period remain unknown. In particular, a red A display during Dual Watch is not enough to infer the RF channel.

`replacement-source/main/h9_main_beacon.c` ports this caller gate. The replacement interface preserves the raw RAM address names so later findings can refine the interpretation without silently changing the condition.
