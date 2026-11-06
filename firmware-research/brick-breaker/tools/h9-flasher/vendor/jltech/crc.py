"""CRC16 used by the vendored JieLi USB protocol code.

Adapted from jl-uboot-tool's CRC module to avoid its crcmod dependency.
"""


def jl_crc16(data, crc=0):
    for value in data:
        crc ^= value << 8
        for _ in range(8):
            crc = ((crc << 1) ^ (0x1021 if crc & 0x8000 else 0)) & 0xFFFF
    return crc
