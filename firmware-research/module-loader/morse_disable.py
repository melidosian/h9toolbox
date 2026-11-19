"""Remove the stock CW menu, bypass its entries, and remove exclusive code."""
from hashlib import sha256
import struct
from pi32v2isa import asm, elf, objdump, table

STOCK_SHA256 = '11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3'
DISABLED_TARGET = 0x6F5CC
ENTRY_GATES = (
    (0x6F96E, '60ff01102cfe', 'if ((r1 & 0x1) == 0)', DISABLED_TARGET),
    (0x6FB30, '60ff01104bfd', 'if ((r1 & 0x1) == 0)', DISABLED_TARGET),
    (0x8769A, '01ff0100cc03', 'if (r0 != 1)', 0x87E38),
)
MENU_TABLE, MENU_ENTRY, MENU_EXIT = 0x76982, 0x76990, 0x76FAE
MENU_ROOT_TAIL = 0x682B6
MENU_ROOT_STOCK = bytes.fromhex('4027 984a 4028 984b 4029 984c 402b 9840 402a 984d')

# All bounds are complete instructions. See MORSE-REMOVAL.md for the callers
# and jump tables used to distinguish these spans from shared code.
REMOVED = (
    (0x6F000, 0x6F03C, 'e17deae70a19a447c5cda120c6355daf35d426fe1ffbf17cd48ad71c1dd06994'),
    (0x6F974, 0x6F9A6, 'c617b8e73bf4fec1480b7afa5bbf509e52af1f5b2e5df79f2c6403cf90f8ec71'),
    (0x6FB36, 0x6FB4C, '7463c8ed3cb6eda2d7d35bdf3cb1a15ee3150a7d9ce2ac472708b1ad641f3bd6'),
    (0x76AA6, 0x76B52, '90b12db87d505c0fb39e69be36c6d78d527babd148f3ff4e690f00abf1174bd9'),
    (0x76BCE, 0x76CDC, '3586103c15a0a1ebb759a84165f22b2e9832c53defd6388bf38a6ecccddd183d'),
    (0x84F88, 0x85610, '252446ad4a9868f77583c4f2cea5705ada113b0070d27726cf7d72a5d5dea2f4'),
    (0x876A0, 0x87710, 'df0bdbffdfa1caa20e28fde77b7a8aba2c95ae740e444ed98fc36f27c41f40ef'),
)


def patches(stock):
    if sha256(stock).hexdigest() != STOCK_SHA256:
        raise ValueError('Morse disable requires exact stock 1.0.38')
    decoder = objdump.decoder_for(table.load(), mcpu='r3')
    result = []
    for site, signature, condition, target in ENTRY_GATES:
        expected = bytes.fromhex(signature)
        if stock[site:site+6] != expected:
            raise ValueError('Morse entry gate does not match stock')
        decoded = decoder.decode(stock, site)
        if not decoded or decoded[2] != 6 or not decoded[3].startswith(condition + ' goto '):
            raise ValueError('Morse entry gate decode mismatch')
        if site + 6 + int(decoded[3].split('goto ')[1]) != target:
            raise ValueError('Morse disabled path changed')
        source = f'goto {target-site-2}\nnop\nnop\n'
        code = bytes(next(s for s in elf.read(asm.assemble_source(source)).sections if s.name == '.text').data)
        branch = decoder.decode(code, 0)
        if len(code) != 6 or branch[2] != 2 or site + 2 + int(branch[3][5:]) != target:
            raise ValueError('Morse replacement branch width/target mismatch')
        result.append((site, expected, code))
    old = bytes.fromhex('9200')
    if stock[MENU_ENTRY:MENU_ENTRY+2] != old:
        raise ValueError('Morse message dispatch differs from stock')
    result.append((MENU_ENTRY, old, struct.pack('<H', (MENU_EXIT-MENU_TABLE)//2)))

    # r1 points to the menu descriptor. Byte 0 is the count and bytes 3–13
    # are stock category IDs. Keep the first seven IDs and compact 8–10.
    # The stock initializer returns with r0 = 10 and otherwise leaves r1 alone.
    if stock[MENU_ROOT_TAIL:MENU_ROOT_TAIL + len(MENU_ROOT_STOCK)] != MENU_ROOT_STOCK:
        raise ValueError('Morse top-level menu list differs from stock')
    source = '''
        r0 = 8
        b[r1+10] = r0
        r0 = 9
        b[r1+11] = r0
        r0 = 0
        b[r1+13] = r0
        r0 = 10
        b[r1+12] = r0
        b[r1+0] = r0
        nop
    '''
    code = bytes(next(s for s in elf.read(asm.assemble_source(source)).sections if s.name == '.text').data)
    if len(code) != len(MENU_ROOT_STOCK):
        raise ValueError('Morse top-level menu initializer changed size')
    result.append((MENU_ROOT_TAIL, MENU_ROOT_STOCK, code))
    for start, end, expected_hash in REMOVED:
        old = stock[start:end]
        if sha256(old).hexdigest() != expected_hash:
            raise ValueError(f'Morse code at {start:#x} differs from stock')
        result.append((start, old, bytes(len(old))))  # nop halfwords
    return result


def apply(stock, image):
    edits = patches(stock)
    # Validate all sites before modifying anything; base may carry other patches.
    for site, old, new in edits:
        if image[site:site+len(old)] != old:
            raise ValueError(f'Morse disable conflicts with existing patch at {site:#x}')
    result = bytearray(image)
    for site, old, new in edits:
        result[site:site+len(old)] = new
    return result
