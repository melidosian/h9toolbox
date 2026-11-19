"""AX.25 v2.0 frame encoding for the H9's port-zero KISS interface."""

from dataclasses import dataclass
import re

CALLSIGN = re.compile(r"[A-Z0-9]{1,6}(?:-(?:[0-9]|1[0-5]))?")
POLL = 0x10
UI, SABM, SABME, DISC, UA, DM, FRMR = 0x03, 0x2f, 0x6f, 0x43, 0x63, 0x0f, 0x87
RR, RNR, REJ = 0x01, 0x05, 0x09
NO_LAYER_3 = 0xf0
NAMES = {UI: "UI", SABM: "SABM", SABME: "SABME", DISC: "DISC", UA: "UA",
         DM: "DM", FRMR: "FRMR", RR: "RR", RNR: "RNR", REJ: "REJ", 0x0d: "SREJ"}


def callsign(text):
    text = text.strip().upper()
    if not CALLSIGN.fullmatch(text):
        raise ValueError("Use an AX.25 callsign with an optional SSID")
    return text.removesuffix("-0")


def kiss_frame(raw):
    return b"\xc0\x00" + raw.replace(b"\xdb", b"\xdb\xdd").replace(b"\xc0", b"\xdb\xdc") + b"\xc0"


def address(text, last, marked):
    base, _, ssid = callsign(text).partition("-")
    field = bytes(ord(char) << 1 for char in base.ljust(6))
    return field + bytes((0x60 | int(ssid or 0) << 1 | int(last) | (0x80 if marked else 0),))


@dataclass(frozen=True)
class Frame:
    source: str
    destination: str
    control: int
    command: bool | None = True
    data: bytes = b""
    pid: int = NO_LAYER_3
    via: tuple[str, ...] = ()
    repeated: int = 0

    @property
    def kind(self):
        if not self.control & 1:
            return 0
        return self.control & (0x0f if self.control & 3 == 1 else 0xef)

    @property
    def poll(self):
        return bool(self.control & POLL)

    @property
    def ns(self):
        return (self.control >> 1) & 7

    @property
    def nr(self):
        return self.control >> 5

    @property
    def label(self):
        if self.kind == 0:
            return f"I[{self.ns}/{self.nr}]"
        name = NAMES.get(self.kind, f"0x{self.control:02X}")
        return f"{name}[{self.nr}]" if self.control & 3 == 1 else name

    def encode(self):
        if len(self.via) > 8:
            raise ValueError("AX.25 supports up to eight digipeaters")
        raw = bytes(address(self.destination, False, self.command is True))
        raw += address(self.source, not self.via, self.command is False)
        for i, call in enumerate(self.via):
            raw += address(call, i == len(self.via) - 1, i < self.repeated)
        raw += bytes((self.control,))
        if self.kind in (0, UI):
            raw += bytes((self.pid,))
        return raw + self.data

    @classmethod
    def decode(cls, raw):
        fields = []
        for i in range(10):
            pos = i * 7
            field = raw[pos:pos + 7]
            if len(field) < 7 or any(v & 1 for v in field[:6]):
                raise ValueError("Invalid AX.25 address")
            base = bytes(v >> 1 for v in field[:6]).decode("ascii").rstrip()
            ssid = (field[6] >> 1) & 15
            fields.append((callsign(f"{base}-{ssid}" if ssid else base), bool(field[6] & 0x80)))
            if field[6] & 1:
                break
        else:
            raise ValueError("Unterminated AX.25 address chain")
        pos += 7
        if len(fields) < 2 or pos >= len(raw):
            raise ValueError("Missing AX.25 source or control")
        destination, dst_c = fields[0]
        source, src_c = fields[1]
        control = raw[pos]
        pos += 1
        pid = NO_LAYER_3
        if not control & 1 or control & 0xef == UI:
            if pos >= len(raw):
                raise ValueError("Missing AX.25 PID")
            pid = raw[pos]
            pos += 1
        via = tuple(call for call, _ in fields[2:])
        return cls(source, destination, control, dst_c if dst_c != src_c else None, raw[pos:], pid,
                   via, sum(marked for _, marked in fields[2:]))
