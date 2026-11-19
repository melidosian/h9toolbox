"""One stop-and-wait AX.25 v2.0 connected session."""

import asyncio
from collections import deque
import logging

from ax25 import DISC, DM, FRMR, NO_LAYER_3, POLL, REJ, RNR, RR, SABM, UA, Frame

LOG = logging.getLogger("h9-kiss-bridge")
T1 = 5.7
T2 = 1.0
T3 = 18.0
BATCH_DELAY = 0.10
MAX_INFO = 80
MAX_QUEUED = 65536
RETRIES = 3


class Link:
    def __init__(self, local, remote, via, send, fits, event):
        self.local, self.remote, self.via = local, remote, via
        self.send, self.fits, self.event = send, fits, event
        self.loop = asyncio.get_running_loop()
        self.state = "new"
        self.vs = self.vr = 0
        self.unacked = None
        self.queue = deque()
        self.queued_bytes = 0
        self.busy = self.reject = False
        self.polling = False
        self.tries = 0
        self.epoch = 0
        self.timer = self.batch = self.ack = None

    @property
    def outstanding(self):
        return int(self.unacked is not None) + sum((len(data) + MAX_INFO - 1) // MAX_INFO for _, data in self.queue)

    def frame(self, control, command=True, data=b"", pid=NO_LAYER_3):
        return Frame(self.local, self.remote, control, command, data, pid, self.via)

    def cancel(self, name):
        handle = getattr(self, name)
        if handle is not None:
            handle.cancel()
            setattr(self, name, None)

    def invalidate(self):
        self.cancel("timer")
        self.epoch += 1

    def transmit(self, frame, timer=None, *, terminal=False):
        epoch = self.epoch

        def valid():
            return terminal or (self.state != "closed" and epoch == self.epoch)

        def completed():
            if timer is not None and valid():
                self.cancel("timer")
                self.timer = self.loop.call_later(T1, timer)

        self.send(frame, valid, completed, self.fail)

    def start(self):
        self.state = "opening"
        self.control_request(SABM)

    def accept(self, frame):
        self.state = "open"
        self.confirm_setup(frame)
        self.event("inbound", b"")
        self.idle()

    def confirm_setup(self, frame):
        self.transmit(self.frame(UA | (POLL if frame.poll else 0), False))

    def control_request(self, kind):
        self.invalidate()
        self.transmit(self.frame(kind | POLL), self.timeout)

    def timeout(self):
        self.timer = None
        if self.tries >= RETRIES:
            if self.state == "open":
                self.disconnect()
            else:
                self.finish(f"*** DISCONNECTED From {self.remote}\r".encode())
            return
        self.tries += 1
        if self.state == "opening":
            self.control_request(SABM)
        elif self.state == "closing":
            self.control_request(DISC)
        elif self.unacked is not None and self.tries > 1 and not self.busy:
            self.retry_data()
        else:
            self.poll_peer()

    def poll_peer(self):
        self.invalidate()
        self.polling = True
        self.send_supervisory(RR, command=True, poll=True, timer=self.timeout)

    def retry_data(self):
        self.invalidate()
        self.polling = True
        self.send_data_frame(poll=True)

    def idle(self):
        self.cancel("timer")
        self.timer = self.loop.call_later(T3, self.start_idle_poll)

    def start_idle_poll(self):
        self.tries = 0
        self.poll_peer()

    def enqueue(self, data, pid):
        if not data or self.state != "open":
            return
        if self.queued_bytes + len(data) > MAX_QUEUED:
            self.fail(ConnectionError("AX.25 send queue full"))
            return
        if self.queue and self.queue[-1][0] == pid:
            self.queue[-1][1].extend(data)
        else:
            self.queue.append((pid, bytearray(data)))
        self.queued_bytes += len(data)
        if self.unacked is None and self.batch is None and not self.busy:
            self.batch = self.loop.call_later(BATCH_DELAY, self.pump)

    def pump(self):
        self.cancel("batch")
        if not self.queue or self.unacked is not None or self.busy or self.state != "open":
            return False
        pid, data = self.queue[0]
        # Fit escaped bytes into one BLE write. The upper bound matches the
        # direct Winlink profile and can shrink for digipeaters or a small MTU.
        control = self.vs << 1 | self.vr << 5 | POLL
        for size in range(min(MAX_INFO, len(data)), 0, -1):
            part = bytes(data[:size])
            if self.fits(self.frame(control, data=part, pid=pid)):
                break
        else:
            self.fail(ConnectionError("BLE write size is too small for AX.25 data"))
            return False
        del data[:size]
        self.queued_bytes -= size
        if not data:
            self.queue.popleft()
        self.unacked = (self.vs, pid, part)
        self.vs = (self.vs + 1) & 7
        self.tries = 0
        self.polling = False
        self.invalidate()
        self.send_data_frame(poll=False)
        return True

    def send_data_frame(self, poll):
        self.cancel("ack")
        ns, pid, data = self.unacked
        control = ns << 1 | self.vr << 5 | (POLL if poll else 0)
        self.transmit(self.frame(control, data=data, pid=pid), self.timeout)

    def send_supervisory(self, kind, *, command=False, poll=False, timer=None):
        self.cancel("ack")
        self.transmit(self.frame(kind | self.vr << 5 | (POLL if poll else 0), command), timer)

    def receive(self, frame):
        kind = frame.kind
        if kind == DISC:
            self.transmit(self.frame(UA | (POLL if frame.poll else 0), False), terminal=True)
            self.finish(f"*** DISCONNECTED From {self.remote}\r".encode())
        elif kind in (DM, FRMR):
            self.finish(f"*** DISCONNECTED From {self.remote}\r".encode())
        elif self.state == "opening" and (
                (kind == UA and frame.command is not True) or (kind == 0 and frame.ns == frame.nr == 0)):
            self.invalidate()
            self.state = "open"
            self.event("connected", b"")
            self.idle()
            if kind == 0:
                self.receive_link(frame)
        elif self.state == "closing" and kind == UA and frame.command is not True:
            self.finish(f"*** DISCONNECTED From {self.remote}\r".encode())
        elif self.state == "open" and kind in (0, RR, RNR, REJ):
            self.receive_link(frame)

    def receive_link(self, frame):
        kind = frame.kind
        final = frame.poll and frame.command is False and self.polling
        polled = frame.poll and frame.command is not False
        advanced = self.unacked is not None and frame.nr == self.vs
        if advanced:
            self.invalidate()
            self.unacked = None
            self.polling = False
            self.tries = 0
        if kind != 0:
            self.busy = kind == RNR

        if kind == 0:
            if frame.ns == self.vr:
                self.vr = (self.vr + 1) & 7
                self.reject = False
                self.event("data", frame.data, frame.pid)
                if not polled:
                    self.cancel("ack")
                    self.ack = self.loop.call_later(T2, self.acknowledge)
            elif frame.ns == ((self.vr - 1) & 7):
                if not polled:
                    self.acknowledge()
            elif not self.reject:
                self.reject = True
                self.send_supervisory(REJ, poll=polled)
                polled = False

        # If a peer keeps reporting the same N(R), recover after an actual
        # poll response or a REJ. Unsolicited duplicate finals do not retry.
        resend = self.unacked is not None and not self.busy and frame.nr == self.unacked[0]
        if resend and (final or kind == REJ):
            if self.tries >= RETRIES:
                self.disconnect()
            else:
                self.tries += 1
                self.retry_data()
        elif self.unacked is None and not self.busy:
            self.polling = False
            if self.batch is not None or not self.pump():
                self.idle()
        elif final or advanced:
            self.polling = False
            self.cancel("timer")
            self.timer = self.loop.call_later(T1, self.timeout)
        if polled and self.state == "open":
            self.send_supervisory(RR, poll=True)

    def acknowledge(self):
        if self.state == "open":
            self.send_supervisory(RR)

    def disconnect(self):
        if self.state in ("new", "closed", "closing"):
            return
        self.state = "closing"
        self.tries = 0
        self.cancel("batch")
        self.cancel("ack")
        self.control_request(DISC)

    def fail(self, error):
        LOG.warning("AX.25 link stopped: %s", error)
        self.finish(f"*** LINK FAILED From {self.remote}\r".encode())

    def finish(self, reason=b""):
        if self.state == "closed":
            return
        self.state = "closed"
        self.invalidate()
        self.cancel("batch")
        self.cancel("ack")
        self.event("closed", reason)
