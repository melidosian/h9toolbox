import argparse
import asyncio
import unittest
from unittest.mock import AsyncMock
from bridge import KissFrames, KissTcpBridge, kiss_payload, parse_args

class FramingTests(unittest.TestCase):
    def test_h9_profile_defaults(self):
        args = parse_args(["--device", "test"])
        self.assertFalse(args.allow_tx)
        self.assertEqual(args.tx_gap, 1.60)
        self.assertEqual(args.rx_guard, 0.70)
        self.assertEqual(args.notify_char, "0000af02-0000-1000-8000-00805f9b34fb")

    def test_tcp_boundaries_and_empty_delimiters(self):
        frame = bytes.fromhex('c0 00 41 db dc db dd c0')
        stream = b'noise\xc0\xc0' + frame + frame
        for split in range(len(stream) + 1):
            decoder = KissFrames()
            self.assertEqual(decoder.feed(stream[:split]) + decoder.feed(stream[split:]), [frame, frame])
        self.assertEqual(kiss_payload(frame), b'A\xc0\xdb')

    def test_oversize_recovers(self):
        decoder = KissFrames(8)
        self.assertEqual(decoder.feed(b'\xc0\x00' + b'x'*9 + b'\xc0\x00A\xc0'), [b'\xc0\x00A\xc0'])

    def test_reject_commands_ports_and_bad_escapes(self):
        for frame in [b'\xc0\xff\xc0', b'\xc0\x01A\xc0', b'\xc0\x10A\xc0', b'\xc0\x00\xdb\xc0', b'\xc0\x00\xdbX\xc0']:
            with self.assertRaises(ValueError):
                kiss_payload(frame)

class ForwardTests(unittest.IsolatedAsyncioTestCase):
    def bridge(self, tx=True):
        bridge = KissTcpBridge(argparse.Namespace(allow_tx=tx, max_frame_size=182, tx_gap=0, rx_guard=0))
        bridge.ble = argparse.Namespace(write_gatt_char=AsyncMock())
        bridge.write_characteristic = argparse.Namespace(max_write_without_response_size=182)
        return bridge

    async def test_one_write_per_complete_frame(self):
        bridge = self.bridge()
        frame = b'\xc0\x00' + b'A'*90 + b'\xc0'
        await bridge.write_client_bytes_to_radio(frame)
        bridge.ble.write_gatt_char.assert_awaited_once_with(bridge.write_characteristic, frame, response=False)

    async def test_no_write_when_disabled_invalid_or_oversize(self):
        bridge = self.bridge(False)
        await bridge.write_client_bytes_to_radio(b'\xc0\x00A\xc0')
        bridge.args.allow_tx = True
        await bridge.write_client_bytes_to_radio(b'\xc0\xff\xc0')
        with self.assertRaises(ConnectionError):
            await bridge.write_client_bytes_to_radio(b'\xc0\x00' + b'A'*180 + b'\xc0')
        bridge.ble.write_gatt_char.assert_not_awaited()

    async def test_pacing_respects_fresh_rx_and_previous_tx(self):
        bridge = self.bridge()
        bridge.args.tx_gap = 0.04
        bridge.args.rx_guard = 0.03
        loop = asyncio.get_running_loop()
        sent_at = []
        bridge.ble.write_gatt_char.side_effect = lambda *a, **kw: sent_at.append(loop.time())
        frame = b"\xc0\x00A\xc0"
        await bridge.write_client_bytes_to_radio(frame)
        pending = asyncio.create_task(bridge.write_client_bytes_to_radio(frame))
        await asyncio.sleep(0.02)
        bridge.on_radio_bytes(None, bytearray(frame))
        seen_at = bridge.last_rx
        await pending
        self.assertGreaterEqual(sent_at[1] - sent_at[0], bridge.args.tx_gap)
        self.assertGreaterEqual(sent_at[1] - seen_at, bridge.args.rx_guard)

    async def test_tcp_fragmented_tx_and_unchanged_rx(self):
        bridge = self.bridge()
        server = await asyncio.start_server(bridge.handle_tcp_client, '127.0.0.1', 0)
        async with server:
            reader, writer = await asyncio.open_connection('127.0.0.1', server.sockets[0].getsockname()[1])
            writer.write(b'\xc0\x00AB')
            await writer.drain()
            await asyncio.sleep(0.02)
            bridge.ble.write_gatt_char.assert_not_awaited()
            writer.write(b'C\xc0\xc0\x00DEF\xc0')
            await writer.drain()
            await asyncio.sleep(0.02)
            self.assertEqual([c.args[1] for c in bridge.ble.write_gatt_char.await_args_list], [b'\xc0\x00ABC\xc0', b'\xc0\x00DEF\xc0'])
            bridge.on_radio_bytes(None, bytearray(b'\xc0\x00RX\xc0'))
            self.assertEqual(await asyncio.wait_for(reader.readexactly(5), 1), b'\xc0\x00RX\xc0')
            writer.close()
            await writer.wait_closed()
            await asyncio.sleep(0.02)

if __name__ == '__main__':
    unittest.main()
