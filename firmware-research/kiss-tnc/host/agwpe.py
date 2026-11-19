"""Loopback AGWPE endpoint for Pat and Paracon, using the H9 AX.25 session."""

import asyncio
from contextlib import suppress
from datetime import datetime
import logging
import struct

from ax25 import DISC, DM, POLL, SABM, SABME, UI, Frame, callsign, kiss_frame
from ax25_link import Link

LOG = logging.getLogger("h9-kiss-bridge")
HEADER = struct.Struct("<B3xcxBx10s10sI4x")
UINT = struct.Struct("<I")
MAX_PAYLOAD = 65536


def call_field(raw):
    text = raw.rstrip(b"\x00 ").decode("ascii")
    return callsign(text) if text else ""


def packet(kind, source="", destination="", data=b"", pid=0):
    return HEADER.pack(0, kind.encode("ascii"), pid, source.encode("ascii"),
                       destination.encode("ascii"), len(data)) + data


def path_and_data(data):
    if not data or data[0] > 8 or len(data) < 1 + 10 * data[0]:
        raise ValueError("Invalid AGWPE digipeater list")
    end = 1 + 10 * data[0]
    return tuple(callsign(call_field(data[i:i + 10])) for i in range(1, end, 10)), data[end:]


class Client:
    def __init__(self, writer):
        self.writer = writer
        self.calls = set()
        self.monitor = self.raw = False

    def send(self, kind, source="", destination="", data=b"", pid=0):
        if self.writer.is_closing():
            return
        if self.writer.transport.get_write_buffer_size() > 4 * MAX_PAYLOAD:
            self.writer.close()
            return
        self.writer.write(packet(kind, source, destination, data, pid))


class Agwpe:
    def __init__(self, *, send_raw, capacity, raw_in_use, allow_tx):
        self.send_raw, self.capacity = send_raw, capacity
        self.raw_in_use, self.allow_tx = raw_in_use, allow_tx
        self.clients = set()
        self.tasks = set()
        self.link = self.owner = None

    def fits(self, frame):
        return len(kiss_frame(frame.encode())) <= self.capacity()

    def send(self, frame, valid, completed, failure):
        async def transfer():
            try:
                if await self.send_raw(frame.encode(), valid):
                    LOG.info("[TX] %-8s %s -> %s  %d bytes", frame.label,
                             frame.source, frame.destination, len(frame.data))
                    completed()
            except Exception as error:
                if valid():
                    failure(error)

        task = asyncio.create_task(transfer())
        self.tasks.add(task)
        task.add_done_callback(self.tasks.discard)

    def send_unconnected(self, frame, client=None, *, response=False):
        def valid():
            return not self.raw_in_use() and (response or self.link is None) and (
                client is None or client in self.clients)

        def failure(error):
            LOG.warning("AX.25 frame dropped: %s", error)

        if self.allow_tx and valid():
            if len(self.tasks) < 32 and self.fits(frame):
                self.send(frame, valid, lambda: None, failure)
            else:
                LOG.warning("Unconnected frame dropped: too large or transmit queue full")

    def new_link(self, owner, local, remote, via):
        def event(kind, data, pid=0):
            if kind in ("connected", "inbound"):
                label, peer = ("With", remote) if kind == "connected" else ("To Station", remote)
                owner.send("C", remote, local, f"*** CONNECTED {label} {peer}\r".encode())
            elif kind == "data":
                owner.send("D", remote, local, data, pid)
            else:
                self.link = self.owner = None
                owner.send("d", remote, local, data or f"*** DISCONNECTED From {remote}\r".encode())

        self.owner = owner
        self.link = Link(local, remote, via, self.send, self.fits, event)
        return self.link

    def receive(self, raw):
        try:
            frame = Frame.decode(raw)
        except ValueError:
            return
        LOG.info("[RX] %-8s %s -> %s  %d bytes", frame.label,
                 frame.source, frame.destination, len(frame.data))
        self.monitor(frame, raw)
        if frame.repeated != len(frame.via):
            return
        link = self.link
        current = link is not None and (frame.destination, frame.source) == (link.local, link.remote)
        if current and frame.kind != SABM:
            link.receive(frame)
        elif self.allow_tx and not self.raw_in_use() and frame.kind in (SABM, SABME, DISC):
            owner = next((c for c in self.clients if frame.destination in c.calls), None)
            if owner is None:
                return
            reply_path = tuple(reversed(frame.via))
            if frame.kind == SABM and (link is None or current):
                if current and link.state == "open" and link.vs == link.vr == 0:
                    link.confirm_setup(frame)
                    return
                if current:
                    link.finish()
                self.new_link(owner, frame.destination, frame.source, reply_path).accept(frame)
            else:
                self.send_unconnected(Frame(frame.destination, frame.source, DM | (frame.control & POLL),
                                            False, via=reply_path), response=True)

    def monitor(self, frame, raw):
        stamp = datetime.now().strftime("%H:%M:%S")
        if frame.kind in (0, UI):
            kind, label = ("U", "UI") if frame.kind == UI else ("I", "I")
            description = f"{label} pid={frame.pid:02X} Len={len(frame.data)}"
            body = frame.data
        else:
            kind, description, body = "S", frame.label, b""
        header = f"Fm {frame.source} To {frame.destination} <{description} >[{stamp}]\r"
        for client in self.clients:
            if client.raw:
                client.send("K", data=b"\0" + raw)
            if client.monitor:
                client.send(kind, frame.source, frame.destination, header.encode("ascii") + body, frame.pid)

    def request(self, client, kind, source, destination, pid, data):
        link = self.link
        owned = link is not None and self.owner is client
        addressed = owned and (source, destination) == (link.local, link.remote)
        if kind == b"R":
            client.send("R", data=struct.pack("<II", 2, 0))
        elif kind == b"G":
            client.send("G", data=b"1;Port1 TD-H9 BLE;")
        elif kind == b"g":
            client.send("g", data=bytes((0, 255, 0, 0, 0, 0, 1, int(link is not None))) + UINT.pack(0))
        elif kind == b"X":
            available = bool(source) and (len(client.calls) < 16 or source in client.calls) and all(
                source not in other.calls for other in self.clients if other is not client)
            if available:
                client.calls.add(source)
            client.send("X", source, data=bytes((available,)))
        elif kind == b"x":
            client.calls.discard(source)
        elif kind == b"m":
            client.monitor = not client.monitor
        elif kind == b"k":
            client.raw = not client.raw
        elif kind in (b"y", b"Y"):
            count = link.outstanding if (addressed if kind == b"Y" else owned) else 0
            client.send(kind.decode("ascii"), source, destination, UINT.pack(count))
        elif kind in (b"C", b"c", b"v"):
            via, extra = path_and_data(data) if kind == b"v" else ((), b"")
            if (extra or not self.allow_tx or self.raw_in_use() or link is not None or
                    source not in client.calls or not destination or source == destination):
                client.send("d", destination, source, f"*** BUSY From {destination}\r".encode())
            else:
                self.new_link(client, source, destination, via).start()
        elif kind == b"D" and addressed:
            link.enqueue(data, pid or 0xf0)
        elif kind == b"d":
            if addressed:
                link.disconnect()
            else:
                client.send("d", destination, source, f"*** DISCONNECTED From {destination}\r".encode())
        elif kind in (b"M", b"V") and source and destination:
            via, info = path_and_data(data) if kind == b"V" else ((), data)
            self.send_unconnected(Frame(source, destination, UI, data=info, pid=pid or 0xf0, via=via), client)
        elif kind == b"K" and self.allow_tx and client.calls and data[:1] == b"\0":
            frame = Frame.decode(data[1:])
            if frame.source in client.calls:
                self.send_unconnected(frame, client)

    async def handle(self, reader, writer):
        client = Client(writer)
        if len(self.clients) >= 8:
            writer.close()
            await writer.wait_closed()
            return
        self.clients.add(client)
        try:
            while True:
                port, kind, pid, source, destination, size = HEADER.unpack(await reader.readexactly(HEADER.size))
                if size > MAX_PAYLOAD:
                    break
                data = await reader.readexactly(size)
                if port == 0:
                    try:
                        self.request(client, kind, call_field(source), call_field(destination), pid, data)
                    except ValueError as error:
                        LOG.warning("Invalid AGWPE request: %s", error)
                        if kind in (b"C", b"c", b"v"):
                            client.send("d", data=b"*** INVALID AX.25 ADDRESS OR PATH\r")
                await writer.drain()
        except (asyncio.IncompleteReadError, OSError):
            pass
        finally:
            self.clients.discard(client)
            if self.owner is client:
                self.link.disconnect()
            writer.close()
            with suppress(OSError):
                await writer.wait_closed()

    async def close(self):
        if self.link is not None:
            self.link.finish()
        for client in self.clients:
            client.writer.close()
        tasks = tuple(self.tasks)
        for task in tasks:
            task.cancel()
        await asyncio.gather(*tasks, return_exceptions=True)
