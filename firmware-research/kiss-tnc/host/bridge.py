#!/usr/bin/env python3
"""Bridge the TD-H9 AF00 packet service to a loopback KISS TCP socket."""

from __future__ import annotations

import argparse
import asyncio
import contextlib
import logging
import math
import re
from uuid import UUID

LOG = logging.getLogger("h9-kiss-bridge")
MIN_TX_GAP = 1.60
RX_TX_GUARD = 0.70


def bluetooth_uuid(value: str) -> str:
    """Normalize 16-bit, 32-bit, or full Bluetooth UUIDs for Bleak."""
    value = value.strip().lower()
    if re.fullmatch(r"[0-9a-f]{4}", value):
        value = f"0000{value}-0000-1000-8000-00805f9b34fb"
    elif re.fullmatch(r"[0-9a-f]{8}", value):
        value = f"{value}-0000-1000-8000-00805f9b34fb"
    try:
        return str(UUID(value))
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"Invalid Bluetooth UUID: {value}") from exc


def parse_args(argv=None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Forward a verified BLE KISS stream to a loopback TCP KISS client."
    )
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument("--scan", action="store_true", help="List nearby Bluetooth LE device identifiers")
    target.add_argument(
        "--device",
        help="Exact BLE device address or platform identifier shown by Bleak",
    )
    parser.add_argument("--service", default="af00", type=bluetooth_uuid)
    parser.add_argument("--write-char", default="af01", type=bluetooth_uuid)
    parser.add_argument("--notify-char", default="af02", type=bluetooth_uuid)
    parser.add_argument(
        "--port", type=int, default=8100, help="Loopback KISS TCP port (default: 8100)"
    )
    parser.add_argument("--max-frame-size", type=int, default=182,
                        help="Maximum complete encoded BLE frame size (default 182)")
    parser.add_argument("--tx-gap", type=float, default=MIN_TX_GAP,
                        help="Minimum quiet seconds between BLE writes (default 1.60)")
    parser.add_argument("--rx-guard", type=float, default=RX_TX_GUARD,
                        help="Minimum quiet seconds after a BLE notification before a write (default 0.70)")
    parser.add_argument(
        "--allow-tx",
        action="store_true",
        help="Forward bytes from the local KISS client to the radio; may transmit over RF",
    )
    args = parser.parse_args(argv)
    if not 1 <= args.port <= 65535:
        parser.error("--port must be between 1 and 65535")
    if not 3 <= args.max_frame_size <= 512:
        parser.error("--max-frame-size must be between 3 and 512")
    if any(not math.isfinite(delay) or delay < 0 for delay in (args.tx_gap, args.rx_guard)):
        parser.error("--tx-gap and --rx-guard must be finite and nonnegative")
    return args


class KissFrames:
    """Bounded streaming framer; preserves encoded escapes and discards empty frames."""
    def __init__(self, limit: int = 4096) -> None:
        self.limit = limit
        self.buffer: bytearray | None = None

    def feed(self, data: bytes) -> list[bytes]:
        frames = []
        for byte in data:
            if byte == 0xC0:
                if self.buffer:
                    frames.append(b"\xc0" + bytes(self.buffer) + b"\xc0")
                self.buffer = bytearray()
            elif self.buffer is not None:
                self.buffer.append(byte)
                if len(self.buffer) + 2 > self.limit:
                    LOG.error("Discarded oversized KISS frame (limit %d)", self.limit)
                    self.buffer = None
        return frames


def kiss_payload(frame: bytes) -> bytes:
    decoded = bytearray()
    escape = False
    for byte in frame[1:-1]:
        if escape:
            if byte not in (0xDC, 0xDD):
                raise ValueError("Invalid KISS escape")
            decoded.append(0xC0 if byte == 0xDC else 0xDB)
            escape = False
        elif byte == 0xDB:
            escape = True
        else:
            decoded.append(byte)
    if escape:
        raise ValueError("Incomplete KISS escape")
    if not decoded or decoded[0] != 0:
        raise ValueError("Only KISS port-0 data is supported; configuration/exit blocked")
    if len(decoded) < 2:
        raise ValueError("Empty KISS data payload")
    return bytes(decoded[1:])


class KissTcpBridge:
    def __init__(self, args: argparse.Namespace) -> None:
        self.args = args
        self.ble = None
        self.write_characteristic = None
        self.radio_queue: asyncio.Queue[bytes] | None = None
        self.client_active = False
        self.last_tx = self.last_rx = -math.inf

    def on_radio_bytes(self, _characteristic: object, data: bytearray) -> None:
        """Queue BLE notifications as-is; KISS frames can span notifications."""
        self.last_rx = asyncio.get_running_loop().time()
        LOG.debug("BLE RX notification, %d bytes", len(data))
        queue = self.radio_queue
        if queue is None:
            return
        try:
            queue.put_nowait(bytes(data))
        except asyncio.QueueFull:
            LOG.error("RX queue full; dropping a BLE notification")

    async def forward_radio_to_client(
        self, queue: asyncio.Queue[bytes], writer: asyncio.StreamWriter
    ) -> None:
        while True:
            data = await queue.get()
            writer.write(data)
            await writer.drain()

    async def write_client_bytes_to_radio(self, frame: bytes) -> None:
        try:
            kiss_payload(frame)
        except ValueError as exc:
            LOG.warning("Dropped KISS frame: %s", exc)
            return
        if not self.args.allow_tx:
            LOG.warning("TX blocked; dropped %d-byte complete KISS frame", len(frame))
            return
        if self.ble is None or self.write_characteristic is None:
            raise ConnectionError("BLE radio is not connected")
        capacity = min(self.args.max_frame_size,
                       self.write_characteristic.max_write_without_response_size)
        if len(frame) > capacity:
            raise ConnectionError(
                f"Complete KISS frame is {len(frame)} bytes; BLE limit is {capacity}. "
                "Frame rejected; H9 writes cannot be split. Reduce AX.25 packet length.")
        loop = asyncio.get_running_loop()
        while (remaining := max(self.last_tx + self.args.tx_gap,
                                self.last_rx + self.args.rx_guard) - loop.time()) > 0:
            await asyncio.sleep(remaining)
        await self.ble.write_gatt_char(self.write_characteristic, frame, response=False)
        self.last_tx = loop.time()
        LOG.debug("BLE TX handed off %d encoded bytes in one write", len(frame))

    async def handle_tcp_client(
        self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter
    ) -> None:
        if self.client_active:
            LOG.warning("Rejected a second TCP client; only one KISS client is supported")
            writer.close()
            await writer.wait_closed()
            return

        self.client_active = True
        queue: asyncio.Queue[bytes] = asyncio.Queue(maxsize=256)
        self.radio_queue = queue
        pump = asyncio.create_task(self.forward_radio_to_client(queue, writer))
        peer = writer.get_extra_info("peername")
        LOG.info("KISS TCP client connected: %s", peer)
        framer = KissFrames()
        try:
            while data := await reader.read(4096):
                for frame in framer.feed(data):
                    await self.write_client_bytes_to_radio(frame)
        except (ConnectionError, OSError) as exc:
            LOG.warning("TCP/BLE forwarding stopped: %s", exc)
        finally:
            if self.radio_queue is queue:
                self.radio_queue = None
            pump.cancel()
            with contextlib.suppress(asyncio.CancelledError, ConnectionError, OSError):
                await pump
            writer.close()
            with contextlib.suppress(ConnectionError, OSError):
                await writer.wait_closed()
            self.client_active = False
            LOG.info("KISS TCP client disconnected")

    async def run(self) -> None:
        from bleak import BleakClient, BleakScanner

        device = await BleakScanner.find_device_by_address(self.args.device, timeout=15)
        if device is None:
            raise RuntimeError(f"BLE device not found: {self.args.device}")

        disconnected = asyncio.Event()
        async with BleakClient(device, disconnected_callback=lambda _: disconnected.set()) as ble:
            self.ble = ble
            service = ble.services.get_service(self.args.service)
            if service is None:
                raise RuntimeError(f"BLE service not found: {self.args.service}")

            write_char = service.get_characteristic(self.args.write_char)
            notify_char = service.get_characteristic(self.args.notify_char)
            if write_char is None:
                raise RuntimeError(f"Writable characteristic not found: {self.args.write_char}")
            if notify_char is None:
                raise RuntimeError(f"Notification characteristic not found: {self.args.notify_char}")

            write_properties = set(write_char.properties)
            if "write-without-response" not in write_properties:
                raise RuntimeError("H9 requires write-without-response")
            if not ({"notify", "indicate"} & set(notify_char.properties)):
                raise RuntimeError("Selected BLE characteristic does not support notifications or indications")

            LOG.info("BLE MTU=%d; single-write capacity=%d; configured limit=%d",
                     ble.mtu_size, write_char.max_write_without_response_size,
                     self.args.max_frame_size)
            self.write_characteristic = write_char
            await ble.start_notify(notify_char, self.on_radio_bytes)
            mode = "TX ENABLED" if self.args.allow_tx else "receive-only (default)"
            LOG.info("BLE connected on AF00; %s", mode)
            LOG.info("TX spacing %.2f s, RX-to-TX guard %.2f s", self.args.tx_gap, self.args.rx_guard)
            LOG.info("Listening only on 127.0.0.1:%d", self.args.port)
            LOG.info("No KISS mode switch or radio configuration command will be sent")

            server = await asyncio.start_server(
                self.handle_tcp_client, host="127.0.0.1", port=self.args.port
            )
            async with server:
                await disconnected.wait()
                raise ConnectionError("Radio disconnected; stopped TCP listener")


async def scan() -> None:
    from bleak import BleakScanner

    devices = await BleakScanner.discover(timeout=8, return_adv=True)
    for address, (device, adv) in sorted(devices.items()):
        name = adv.local_name or device.name or "(unnamed)"
        print(f"{address}  {name}")


def main() -> None:
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s %(levelname)s %(message)s",
    )
    args = parse_args()
    try:
        asyncio.run(scan() if args.scan else KissTcpBridge(args).run())
    except KeyboardInterrupt:
        LOG.info("Stopped")
    except ImportError as exc:
        LOG.error("Install the Bluetooth dependency with python -m pip install -r requirements.txt")
        raise SystemExit(1) from exc
    except Exception as exc:
        LOG.error("Bridge stopped: %s", exc)
        raise SystemExit(1) from exc


if __name__ == "__main__":
    main()
