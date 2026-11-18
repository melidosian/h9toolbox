"""Pinned stock CW-entry/key disable; does not reclaim code or hide menu labels.

Use the stock disabled branch for both terminal entry paths, and the shared
menu handler's existing default epilogue for CW transmit-message dispatch.
No stock settings, allocator, cleanup, RF or audio routine is overwritten.
"""
from hashlib import sha256
import struct
from pi32v2isa import asm, elf, objdump, table

STOCK_SHA256 = '11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3'
ENTRY_GATES = ((0x6F96E, bytes.fromhex('60ff01102cfe')),
               (0x6FB30, bytes.fromhex('60ff01104bfd')))
DISABLED_TARGET = 0x6F5CC
KEY_GATE = (0x876B2, bytes.fromhex('60ff0100c003'), 0x87E38)
MENU_TABLE, MENU_ENTRY, MENU_EXIT = 0x76982, 0x76990, 0x76FAE


def patches(stock):
    if sha256(stock).hexdigest() != STOCK_SHA256:
        raise ValueError('Morse disable requires exact stock 1.0.38')
    decoder = objdump.decoder_for(table.load(), mcpu='r3')
    result = []
    for site, expected in ENTRY_GATES:
        if stock[site:site+6] != expected:
            raise ValueError('Morse entry gate does not match stock')
        decoded = decoder.decode(stock, site)
        if not decoded or decoded[2] != 6 or not decoded[3].startswith('if ((r1 & 0x1) == 0) goto '):
            raise ValueError('Morse entry gate decode mismatch')
        if site + 6 + int(decoded[3].split('goto ')[1]) != DISABLED_TARGET:
            raise ValueError('Morse disabled path changed')
        source = f'goto {DISABLED_TARGET-site-2}\nnop\nnop\n'
        code = bytes(next(s for s in elf.read(asm.assemble_source(source)).sections if s.name == '.text').data)
        branch = decoder.decode(code, 0)
        if len(code) != 6 or branch[2] != 2 or site + 2 + int(branch[3][5:]) != DISABLED_TARGET:
            raise ValueError('Morse replacement branch width/target mismatch')
        result.append((site, expected, code))
    site, expected, target = KEY_GATE
    if stock[site:site+6] != expected:
        raise ValueError('Morse key gate does not match stock')
    decoded = decoder.decode(stock, site)
    if not decoded or decoded[2] != 6 or not decoded[3].startswith('if ((r0 & 0x1) == 0) goto '):
        raise ValueError('Morse key gate decode mismatch')
    if site + 6 + int(decoded[3].split('goto ')[1]) != target:
        raise ValueError('Morse key disabled path changed')
    source = f'goto {target-site-2}\nnop\nnop\n'
    code = bytes(next(s for s in elf.read(asm.assemble_source(source)).sections if s.name == '.text').data)
    branch = decoder.decode(code, 0)
    if len(code) != 6 or branch[2] != 2 or site + 2 + int(branch[3][5:]) != target:
        raise ValueError('Morse key replacement branch width/target mismatch')
    result.append((site, expected, code))
    old = bytes.fromhex('9200')
    if stock[MENU_ENTRY:MENU_ENTRY+2] != old:
        raise ValueError('Morse message dispatch differs from stock')
    result.append((MENU_ENTRY, old, struct.pack('<H', (MENU_EXIT-MENU_TABLE)//2)))
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
