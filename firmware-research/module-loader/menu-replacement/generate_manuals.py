"""Preserve the pinned stock manual QR images losslessly in a compact palette."""
from hashlib import sha256
from pathlib import Path
import sys

STOCK_SHA256 = "11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3"
def encode(image):
    colors = list(dict.fromkeys(int.from_bytes(image[i:i+2], "big") for i in range(0, len(image), 2)))
    bits = (len(colors) - 1).bit_length()
    packed = bytearray((len(image) // 2 * bits + 7) // 8)
    for pixel in range(len(image) // 2):
        index = colors.index(int.from_bytes(image[pixel*2:pixel*2+2], "big"))
        bit = pixel * bits
        packed[bit // 8] |= (index << (bit % 8)) & 255
        if bit % 8 + bits > 8: packed[bit // 8 + 1] |= index >> (8 - bit % 8)
    # Decode every pixel as the firmware does; no threshold or lossy resizing.
    for pixel in range(len(image) // 2):
        bit = pixel * bits
        index = packed[bit // 8] >> (bit % 8)
        if bit % 8 + bits > 8: index |= packed[bit // 8 + 1] << (8 - bit % 8)
        assert colors[index & ((1 << bits) - 1)] == int.from_bytes(image[pixel*2:pixel*2+2], "big")
    return colors, bits, packed

def generate(stock):
    if sha256(stock).hexdigest() != STOCK_SHA256: raise ValueError("Expected pinned factory v1.0.38")
    lines = ['/* Generated from the three factory manual QR assets, losslessly. */',
             '#ifndef RADIO_MANUAL_IMAGES_H', '#define RADIO_MANUAL_IMAGES_H', '#include <stdint.h>']
    pages = [('GMRS MANUAL',0xA2EB2),('HAM MANUAL',0xA54FA),('VIDEO GUIDE',0xA7B42)]
    for n, (_, offset) in enumerate(pages):
        colors, bits, packed = encode(stock[offset:offset+9800])
        lines.append(f'static const uint16_t manual_palette_{n}[] = {{'+','.join(hex(c) for c in colors)+'};')
        lines.append(f'static const uint8_t manual_pixels_{n}[] = {{')
        for at in range(0, len(packed), 24): lines.append('    '+','.join(hex(c) for c in packed[at:at+24])+',')
        lines.append('};')
    lines += ['struct radio_manual_image { const char *name; const uint16_t *palette; const uint8_t *pixels; unsigned bits; };',
              'static const struct radio_manual_image manual_images[] = {']
    for n, (name, offset) in enumerate(pages):
        _, bits, _ = encode(stock[offset:offset+9800])
        lines.append(f'    {{"{name}", manual_palette_{n}, manual_pixels_{n}, {bits}}},')
    lines += ['};', '#endif', '']
    return '\n'.join(lines)

if __name__ == '__main__':
    Path(__file__).with_name('radio_manual_images.h').write_text(generate(Path(sys.argv[1]).read_bytes()), encoding='utf-8')
