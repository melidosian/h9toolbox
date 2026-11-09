#!/usr/bin/env python3
"""Doom music (MIDI) as an H9MU event list for the TD-H9's chiptune player.

    python3 -I music.py <freedoom1.wad | song.mid> [--lump D_E1M1] -o e1m1.h9mu

The radio (port/td_music.c) plays square-wave voices and one drum voice at
11025 Hz in chunks of 64 samples; times here are counted in those chunks
(5.8 ms). Voices are allocated here, so the radio only follows the list.

H9MU layout (little-endian):
    0   "H9MU"
    4   u16 version (1)
    6   u8  melodic voices, u8 drum voice index
    8   u32 event bytes
    12  u32 loop length in chunks
    16  char[16] name
    32  events: varint delta (chunks), then a command byte
          0x1v note on, voice v:  note, volume (0..15)
          0x0v note off, voice v
          0x2v drum hit, voice v: drum note, volume (0..15)
          0xFF end of song (the radio loops to the first event)

The song data is Freedoom's (BSD licence); this tool only rearranges it.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import struct

RATE = 11025
CHUNK = 64
VOICES = 7          # melodic square-wave voices
DRUM_VOICE = 7      # the noise / kick voice
DRUM_CHANNEL = 9


def wad_lump(path: Path, name: str) -> bytes:
    data = path.read_bytes()
    if data[:4] not in (b"IWAD", b"PWAD"):
        return data  # a plain .mid file
    count, table = struct.unpack_from("<II", data, 4)
    for i in range(count):
        offset, size, raw = struct.unpack_from("<II8s", data, table + 16 * i)
        if raw.rstrip(b"\0").decode() == name:
            return data[offset:offset + size]
    raise SystemExit(f"{name} not in {path}")


def vlq(data: bytes, i: int) -> tuple[int, int]:
    value = 0
    while True:
        c = data[i]
        i += 1
        value = value << 7 | c & 0x7F
        if c < 0x80:
            return value, i


def midi_events(midi: bytes):
    """All channel events as (tick, kind, channel, a, b), and the division and tempo map."""
    if midi[:4] != b"MThd":
        raise SystemExit("not a MIDI file (MUS is not supported here)")
    _, tracks, division = struct.unpack_from(">HHH", midi, 8)
    if division & 0x8000:
        raise SystemExit("SMPTE time division is not supported")
    i = 8 + struct.unpack_from(">I", midi, 4)[0]
    events, tempos = [], []
    for _ in range(tracks):
        if midi[i:i + 4] != b"MTrk":
            raise SystemExit("bad track chunk")
        end = i + 8 + struct.unpack_from(">I", midi, i + 4)[0]
        j, tick, status = i + 8, 0, 0
        while j < end:
            delta, j = vlq(midi, j)
            tick += delta
            if midi[j] & 0x80:
                status = midi[j]
                j += 1
            if status == 0xFF:
                kind = midi[j]
                length, j = vlq(midi, j + 1)
                if kind == 0x51:
                    tempos.append((tick, int.from_bytes(midi[j:j + 3], "big")))
                j += length
            elif status in (0xF0, 0xF7):
                length, j = vlq(midi, j)
                j += length
            else:
                high, channel = status & 0xF0, status & 0x0F
                if high in (0xC0, 0xD0):
                    events.append((tick, high, channel, midi[j], 0))
                    j += 1
                else:
                    events.append((tick, high, channel, midi[j], midi[j + 1]))
                    j += 2
        i = end
    return events, division, sorted(tempos)


def tick_to_chunk(division: int, tempos: list[tuple[int, int]]):
    def convert(tick: int) -> int:
        seconds, last, tempo = 0.0, 0, 500000
        for at, value in tempos:
            if at >= tick:
                break
            seconds += (at - last) * tempo / division / 1e6
            last, tempo = at, value
        seconds += (tick - last) * tempo / division / 1e6
        return round(seconds * RATE / CHUNK)
    return convert


def convert(midi: bytes, name: str) -> tuple[bytes, dict]:
    events, division, tempos = midi_events(midi)
    to_chunk = tick_to_chunk(division, tempos)
    # note offs before note ons at the same tick, so a voice frees in time
    events.sort(key=lambda e: (e[0], 0 if e[1] == 0x80 or (e[1] == 0x90 and e[4] == 0) else 1))
    volume = [100] * 16
    voices = [None] * VOICES        # (channel, note, start chunk) per voice
    out = bytearray()
    last_chunk = 0
    stats = {"notes": 0, "stolen": 0, "drums": 0, "dropped": 0}

    def emit(chunk: int, command: bytes):
        nonlocal last_chunk
        delta = chunk - last_chunk
        last_chunk = chunk
        while True:
            byte = delta & 0x7F
            delta >>= 7
            out.append(byte | (0x80 if delta else 0))
            if not delta:
                break
        out.extend(command)

    end_chunk = 0
    for tick, kind, channel, a, b in events:
        chunk = to_chunk(tick)
        end_chunk = max(end_chunk, chunk)
        if kind == 0xB0 and a == 7:
            volume[channel] = b
            continue
        on = kind == 0x90 and b > 0
        off = kind == 0x80 or (kind == 0x90 and b == 0)
        level = min(15, max(1, b * volume[channel] // (127 * 127 // 15))) if on else 0
        if channel == DRUM_CHANNEL:
            if on:
                emit(chunk, bytes((0x20 | DRUM_VOICE, a, level)))
                stats["drums"] += 1
            continue
        if off:
            for v, held in enumerate(voices):
                if held and held[0] == channel and held[1] == a:
                    voices[v] = None
                    emit(chunk, bytes((0x00 | v,)))
                    break
            continue
        if not on:
            continue
        if not 24 <= a <= 108:
            stats["dropped"] += 1
            continue
        free = next((v for v, held in enumerate(voices) if held is None), None)
        if free is None:
            free = min(range(VOICES), key=lambda v: voices[v][2])  # steal the oldest
            stats["stolen"] += 1
        voices[free] = (channel, a, chunk)
        emit(chunk, bytes((0x10 | free, a, level)))
        stats["notes"] += 1
    for v, held in enumerate(voices):
        if held:
            emit(end_chunk, bytes((0x00 | v,)))
    emit(end_chunk + 1, b"\xFF")
    header = b"H9MU" + struct.pack("<HBBII", 1, VOICES, DRUM_VOICE, len(out), end_chunk + 1)
    header += name.encode()[:16].ljust(16, b"\0")
    stats.update(bytes=len(out), seconds=round((end_chunk + 1) * CHUNK / RATE, 1))
    return header + bytes(out), stats


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("source", type=Path)
    parser.add_argument("--lump", default="D_E1M1")
    parser.add_argument("-o", "--output", type=Path, required=True)
    args = parser.parse_args()
    data, stats = convert(wad_lump(args.source, args.lump), args.lump)
    args.output.write_bytes(data)
    print(f"{args.output}: {len(data)} bytes, {stats}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
