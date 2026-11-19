import asyncio
import struct
import unittest
from unittest.mock import patch

from agwpe import HEADER, Agwpe, packet
from ax25 import DISC, DM, POLL, REJ, RNR, RR, SABM, UA, UI, Frame, kiss_frame
from bridge import KissTcpBridge, kiss_payload, parse_args

LOCAL, REMOTE = "N0CALL-7", "N1TEST-10"


class CodecTests(unittest.TestCase):
    def test_known_control_frame_and_escaped_data(self):
        raw = bytes.fromhex("9c62a88aa6a8f49c60868298986f3f")
        self.assertEqual(Frame(LOCAL, REMOTE, SABM | POLL).encode(), raw)
        self.assertEqual(Frame.decode(raw), Frame(LOCAL, REMOTE, SABM | POLL))
        frame = Frame(REMOTE, LOCAL, 0x40, False, b"\xc0\xdb\xff\x00", via=("DIGI",), repeated=1)
        self.assertEqual(Frame.decode(kiss_payload(kiss_frame(frame.encode()))), frame)
        for bad in (raw[:7], raw[:14], raw[:-1] + b"\0"):
            with self.assertRaises(ValueError):
                Frame.decode(bad)


class AgwpeTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.air = asyncio.Queue()
        self.bridge = KissTcpBridge(parse_args(["--device", "test", "--allow-tx", "--tx-gap", "0", "--rx-guard", "0"]))
        self.bridge.ble = self
        self.bridge.write_characteristic = type("Characteristic", (), {"max_write_without_response_size": 182})()
        self.agw = Agwpe(send_raw=self.bridge.send_ax25, capacity=self.bridge.capacity,
                         raw_in_use=lambda: self.bridge.client_active, allow_tx=True)
        self.bridge.agw = self.agw
        self.server = await asyncio.start_server(self.agw.handle, "127.0.0.1", 0)
        self.port = self.server.sockets[0].getsockname()[1]
        self.reader, self.writer = await self.client()
        self.timers = patch.multiple("ax25_link", T1=0.16, T2=0.025, T3=2, BATCH_DELAY=0.018)
        self.timers.start()

    async def asyncTearDown(self):
        self.timers.stop()
        self.writer.close()
        await self.writer.wait_closed()
        self.server.close()
        await self.server.wait_closed()
        await self.agw.close()

    async def client(self):
        reader, writer = await asyncio.open_connection("127.0.0.1", self.port)
        self.addAsyncCleanup(writer.wait_closed)
        self.addCleanup(writer.close)
        return reader, writer

    async def write_gatt_char(self, _char, data, response):
        self.assertFalse(response)
        await self.air.put(Frame.decode(kiss_payload(data)))

    def request(self, kind, data=b"", dst=REMOTE, pid=0, writer=None):
        (writer or self.writer).write(packet(kind, LOCAL, dst, data, pid))

    async def read(self, wanted, reader=None):
        reader = reader or self.reader
        header = await asyncio.wait_for(reader.readexactly(36), 1)
        port, kind, pid, source, dest, size = HEADER.unpack(header)
        self.assertEqual((port, kind), (0, wanted.encode()))
        return (await asyncio.wait_for(reader.readexactly(size), 1),
                source.rstrip(b"\0").decode(), dest.rstrip(b"\0").decode(), pid)

    async def sent(self, kind):
        frame = await asyncio.wait_for(self.air.get(), 1)
        self.assertEqual(frame.kind, kind)
        return frame

    def heard(self, control, data=b"", command=False, via=(), repeated=0):
        frame = kiss_frame(Frame(REMOTE, LOCAL, control, command, data, via=via, repeated=repeated).encode())
        self.bridge.on_radio_bytes(None, bytearray(frame[:4]))
        self.bridge.on_radio_bytes(None, bytearray(frame[4:]))

    async def register(self):
        self.request("X", dst="")
        self.assertEqual((await self.read("X"))[0], b"\x01")

    async def establish(self, *, greeting=False):
        await self.register()
        self.request("C")
        self.assertTrue((await self.sent(SABM)).poll)
        if greeting:
            self.heard(0, b"hello", command=True)
        else:
            self.heard(UA | POLL)
        msg, source, dest, _ = await self.read("C")
        self.assertEqual((msg, source, dest), (b"*** CONNECTED With N1TEST-10\r", REMOTE, LOCAL))

    async def test_pat_queue_batch_and_close(self):
        await self.establish(greeting=True)
        self.assertEqual((await self.read("D"))[0], b"hello")
        self.request("D", b"header\x00", pid=0xf0)
        self.request("Y")
        self.assertEqual(struct.unpack("<I", (await self.read("Y"))[0]), (1,))
        body = b"\xc0\xdb\xff" + b"b" * 95
        self.request("D", body)
        first = await self.sent(0)
        self.assertEqual((first.ns, first.nr, first.data), (0, 1, (b"header\x00" + body)[:80]))
        self.request("Y")
        self.assertEqual(struct.unpack("<I", (await self.read("Y"))[0]), (2,))
        self.heard(RR | 0x20)
        second = await self.sent(0)
        self.assertEqual((second.ns, second.data), (1, (b"header\x00" + body)[80:]))
        self.heard(RR | 0x40)
        self.request("Y")
        self.assertEqual(struct.unpack("<I", (await self.read("Y"))[0]), (0,))
        self.request("d")
        self.assertTrue((await self.sent(DISC)).poll)
        self.heard(UA | POLL)
        self.assertIn(b"DISCONNECTED", (await self.read("d"))[0])

    async def test_poll_then_retry_and_reject_once(self):
        await self.establish()
        self.request("D", b"login")
        self.assertEqual((await self.sent(0)).data, b"login")
        poll = await self.sent(RR)
        self.assertTrue(poll.poll)
        self.heard(RR | POLL)
        retry = await self.sent(0)
        self.assertEqual((retry.ns, retry.poll, retry.data), (0, True, b"login"))
        self.heard(RR | POLL | 0x20)
        self.heard(2, b"out of order", command=True)
        self.assertEqual((await self.sent(REJ)).nr, 0)
        self.heard(2, b"repeat", command=True)
        self.heard(2 | POLL, b"polled", command=True)
        reply = await self.sent(RR)
        self.assertEqual((reply.nr, reply.poll, reply.command), (0, True, False))
        self.heard(0, b"zero", command=True)
        self.assertEqual((await self.read("D"))[0], b"zero")
        self.assertEqual((await self.sent(RR)).nr, 1)
        self.heard(DISC | POLL, command=True)
        self.assertTrue((await self.sent(UA)).poll)
        self.assertIn(b"DISCONNECTED", (await self.read("d"))[0])

    async def test_paracon_ports_monitor_and_inbound(self):
        self.request("G", dst="")
        self.assertEqual((await self.read("G"))[0], b"1;Port1 TD-H9 BLE;")
        self.request("g", dst="")
        self.assertEqual((await self.read("g"))[0][6], 1)
        self.request("m", dst="")
        self.request("R", dst="")
        await self.read("R")
        self.heard(UI, b"beacon\r")
        data, source, dest, pid = await self.read("U")
        self.assertTrue(data.startswith(b"Fm N1TEST-10 To N0CALL-7 <UI pid=F0 Len=7 >["))
        self.assertEqual((data.split(b"\r", 1)[1], source, dest, pid), (b"beacon\r", REMOTE, LOCAL, 0xf0))
        self.request("m", dst="")
        await self.register()
        self.heard(SABM | POLL, command=True, via=("DIGI",), repeated=1)
        self.assertEqual((await self.sent(UA)).via, ("DIGI",))
        self.assertTrue((await self.read("C"))[0].startswith(b"*** CONNECTED To "))
        self.heard(SABM | POLL, command=True, via=("DIGI",), repeated=1)
        await self.sent(UA)
        self.heard(DISC | POLL, command=True, via=("DIGI",), repeated=1)
        await self.sent(UA)
        await self.read("d")

    async def test_read_only_and_direct_client_reserve_radio(self):
        await self.register()
        self.agw.allow_tx = False
        self.request("C")
        self.assertIn(b"BUSY", (await self.read("d"))[0])
        self.heard(SABM | POLL, command=True)
        self.agw.allow_tx = True
        self.bridge.client_active = True
        self.request("C")
        self.assertIn(b"BUSY", (await self.read("d"))[0])
        self.assertTrue(self.air.empty())

    async def test_remote_busy_pauses_queued_data(self):
        await self.establish()
        self.request("D", b"x" * 81)
        self.assertEqual(len((await self.sent(0)).data), 80)
        self.heard(RNR | 0x20)
        self.assertTrue((await self.sent(RR)).poll)
        self.heard(RR | POLL | 0x20)
        second = await self.sent(0)
        self.assertEqual((second.ns, second.data), (1, b"x"))
        self.heard(DM)
        await self.read("d")

    async def test_unchanged_final_responses_exhaust_retries(self):
        await self.establish()
        self.request("D", b"login")
        await self.sent(0)
        self.assertTrue((await self.sent(RR)).poll)
        for _ in range(2):
            self.heard(RR | POLL)
            self.assertTrue((await self.sent(0)).poll)
        self.heard(RR | POLL)
        self.assertTrue((await self.sent(DISC)).poll)
        self.heard(UA | POLL)
        await self.read("d")

    async def test_digipeated_escaped_data_fits_one_ble_write(self):
        self.bridge.write_characteristic.max_write_without_response_size = 111
        await self.register()
        self.request("v", b"\x01" + b"DIGI".ljust(10, b"\0"))
        self.assertEqual((await self.sent(SABM)).via, ("DIGI",))
        self.heard(UA | POLL, via=("DIGI",), repeated=1)
        await self.read("C")
        body = b"\xc0\xdb" * 40
        self.request("D", body)
        parts = []
        while sum(map(len, parts)) < len(body):
            frame = await self.sent(0)
            self.assertEqual(frame.via, ("DIGI",))
            self.assertLessEqual(len(kiss_frame(frame.encode())), 111)
            parts.append(frame.data)
            self.heard(RR | ((frame.ns + 1) & 7) << 5, via=("DIGI",), repeated=1)
        self.assertGreater(len(parts), 1)
        self.assertEqual(b"".join(parts), body)
