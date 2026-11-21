"""Replace the pinned native root-menu function with the module loader entry."""
from hashlib import sha256

STOCK_SHA256 = "11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3"
ROOT_START, ROOT_END = 0x6D326, 0x6D38E
ROOT_POINTER = 0xB6BC4
IMAGE_BASE = 0x01E00100
REMOVED_UI = (
    ("list builder", 0x6811E, 0x682CC),
    ("root", ROOT_START, ROOT_END),
    ("category entry", 0x6ECAE, 0x6EEB2),
    ("setting getter", 0x6F358, 0x70650),
    ("setting saver", 0x73496, 0x74C32),
)
UI_POINTERS = ((0xB6BC4, ROOT_START), (0xB6C00, 0x6ECAE),
               (0xB6C3C, 0x6F358), (0xB6C60, 0x73496))


def apply(stock: bytes, image: bytearray, entry: int, crc_entry: int,
          far_address: int, far_length: int, far_crc: int, assembler, *, preceding_patches=()) -> dict:
    if sha256(stock).hexdigest() != STOCK_SHA256:
        raise ValueError("Native menu replacement requires factory 1.0.38")
    for site, target in UI_POINTERS:
        if int.from_bytes(stock[site:site + 4], "little") != IMAGE_BASE + target:
            raise ValueError("Native menu dispatch record changed")
    expected = bytearray(stock)
    for site, old, new in preceding_patches:
        if len(old) != len(new) or stock[site:site+len(old)] != old:
            raise ValueError("Unverified preceding menu patch")
        expected[site:site+len(old)] = new
    for name, start, end in REMOVED_UI:
        if image[start:end] != expected[start:end]:
            raise ValueError(f"Native {name} replacement overlaps an existing patch")
    code = assembler(f"""
        [--sp] = {{rets, r4}}
        r0 = {far_address:#x}
        r1 = {far_length}
        r4 = {crc_entry:#x}
        call r4
        r1 = {far_crc}
        if (r0 != r1) goto menu_recovery
        r4 = {entry:#x}
        call r4
        {{pc, r4}} = [sp++]
menu_recovery:
        r4 = 0x02000d68
        call r4
menu_recovery_wait:
        goto menu_recovery_wait
""")
    if len(code) > ROOT_END - ROOT_START:
        raise ValueError("Native menu entry exceeds the removed root function")
    image[ROOT_START:ROOT_END] = code + bytes(ROOT_END - ROOT_START - len(code))
    redirect = assembler(f"r3 = {IMAGE_BASE + ROOT_START:#x}\n goto r3\n")
    removed = []
    for name, start, end in REMOVED_UI:
        if start != ROOT_START:
            image[start:end] = redirect + bytes(end - start - len(redirect))
        removed.append({"name": name, "app_start": hex(start), "app_end": hex(end),
                        "bytes": end - start, "original_sha256": sha256(stock[start:end]).hexdigest()})
    return {"removed_app_start": hex(ROOT_START), "removed_app_end": hex(ROOT_END),
            "root_removed_bytes": ROOT_END - ROOT_START, "removed_bytes": sum(end-start for _,start,end in REMOVED_UI),
            "removed_functions": removed, "entry": hex(entry),
            "far_crc16": hex(far_crc), "guard_bytes": len(code),
            "bad_far_payload": "USB update recovery", "hardware_tested": False}
