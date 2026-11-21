"""Mark the pinned RX15 modem image in its version replies and boot banner."""
from hashlib import sha256


MODEM_VERSION = "V1.0.15X"
IMAGE_NAME = f"APRS-{MODEM_VERSION[1:]}.bin"
RX15_SHA256 = "3a70e2f48283cfd0d06fb29a08f462fc0b8302898e09d41946d5f74205daf741"


def patch_modem(rx15: bytes) -> bytes:
    if sha256(rx15).hexdigest() != RX15_SHA256:
        raise ValueError("Expected the approved APRS 1.0.15 RX15 and unsigned-byte image")
    result = bytearray(rx15)

    # Selectors 0 and 1 copy the new nine-byte version including its NUL.
    # The protocol string now uses the identical, aligned boot-banner literal
    # at 0x08005EA8. Both Thumb ADR instructions still reach that address.
    changes = (
        (0x3AD6, b"\x08\x20", b"\x09\x20"),
        (0x3AE0, b"\x1a\xa1", b"\xf1\xa1"),
        (0x3B00, b"\x08\x20", b"\x09\x20"),
        (0x3B16, b"\x0d\xa1", b"\xe4\xa1"),
        (0x3B44, b"V1.0.15\0V1.0.0\0\0", MODEM_VERSION.encode("ascii").ljust(16, b"\0")),
        (0x3E90, b"Software Version: %s\r\n\0\0", b"Software Version: %sX\r\n\0"),
    )
    for offset, old, new in changes:
        if len(old) != len(new) or rx15[offset:offset + len(old)] != old:
            raise ValueError(f"Unexpected modem version patch at {offset:#x}")
        result[offset:offset + len(new)] = new
    return bytes(result)
