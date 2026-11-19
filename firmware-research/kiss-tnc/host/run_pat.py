#!/usr/bin/env python3
"""Run the H9 bridge and Pat web UI from one local terminal."""

import argparse
from contextlib import ExitStack
import json
from pathlib import Path
import re
import shutil
import socket
import subprocess
import sys
from tempfile import TemporaryDirectory
import time
from urllib.request import urlopen
import webbrowser

HERE = Path(__file__).resolve().parent
WEB = "http://127.0.0.1:8080/"
AGWPE = "127.0.0.1:8000"
CALLSIGN = re.compile(r"[A-Z0-9]{1,6}(?:-(?:[0-9]|1[0-5]))?")


def stop(child):
    if child.poll() is None:
        child.terminate()
        try:
            child.wait(timeout=5)
        except subprocess.TimeoutExpired:
            child.kill()
            child.wait()


def verify_ports():
    for port in (8000, 8080, 8100):
        with socket.socket() as sock:
            sock.settimeout(1)
            if sock.connect_ex(("127.0.0.1", port)) == 0:
                raise RuntimeError(f"Port {port} is already in use")


def ensure_running(children):
    for name, child in children.items():
        if (result := child.poll()) is not None:
            raise RuntimeError(f"{name} exited with status {result}. See the terminal above.")


def wait_for_radio(children):
    deadline = time.monotonic() + 40
    while time.monotonic() < deadline:
        ensure_running(children)
        try:
            with socket.create_connection(("127.0.0.1", 8000), timeout=1):
                return
        except OSError:
            pass
        time.sleep(0.3)
    raise RuntimeError("Timed out waiting for the H9 Bluetooth KISS connection")


def open_pat(children):
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        ensure_running(children)
        try:
            with urlopen(WEB, timeout=1):
                break
        except OSError:
            time.sleep(0.3)
    else:
        print(f"Pat's web interface is still starting. Open {WEB} when ready.", flush=True)
        return
    if not webbrowser.open(WEB):
        print(f"Open {WEB} in your browser.", flush=True)


def run(args):
    verify_ports()
    pat = shutil.which(args.pat)
    if not pat:
        raise RuntimeError("Install Pat or use --pat to select its executable")

    config = json.loads(args.pat_config.read_text(encoding="utf-8-sig"))
    call = config.get("mycall", "").strip().upper()
    if not CALLSIGN.fullmatch(call):
        raise ValueError("Set your amateur callsign in the Pat configuration first")
    config.setdefault("agwpe", {}).update(addr=AGWPE, radio_port=0)
    config.update(mycall=call, schedule={}, listen=[], version_reporting_disabled=True)
    if args.gateway:
        gateway = args.gateway.upper()
        if not CALLSIGN.fullmatch(gateway):
            raise ValueError("Use an AX.25 callsign with an optional SSID for --gateway")
        config.setdefault("connect_aliases", {})[f"H9 {gateway}"] = f"ax25+agwpe:///{gateway}"

    with ExitStack() as cleanup:
        session = Path(cleanup.enter_context(TemporaryDirectory(prefix="h9-pat-")))
        profile = session / "pat.json"
        with profile.open("x", encoding="utf-8") as output:
            json.dump(config, output)
        profile.chmod(0o600)
        children = {}

        def start(name, command):
            child = subprocess.Popen(command)
            children[name] = child
            cleanup.callback(stop, child)

        radio = [sys.executable, "-u", str(HERE / "bridge.py"), "--device", args.device]
        if args.allow_tx:
            radio.append("--allow-tx")
        start("Bluetooth bridge", radio)
        wait_for_radio(children)
        pat_cmd = [pat, "--config", str(profile), "--log", str(session / "pat.log")]
        if args.mbox:
            pat_cmd += ["--mbox", str(args.mbox.resolve())]
        start("Pat", pat_cmd + ["http", "--addr", "127.0.0.1:8080"])
        print(f"Open {WEB} on this computer. The terminal shows live AX.25 RX and TX.", flush=True)
        if not args.allow_tx:
            print("Receive-only. Restart with --allow-tx to use Pat's Connect button.", flush=True)
        if args.open_browser:
            open_pat(children)
        while True:
            ensure_running(children)
            time.sleep(0.5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", required=True, help="Device identifier from bridge.py --scan")
    parser.add_argument("--pat-config", required=True, type=Path, help="PAT_CONFIG_PATH shown by pat env")
    parser.add_argument("--pat", default="pat", help="Pat program name or path")
    parser.add_argument("--mbox", type=Path, help="Use an alternate Pat mailbox")
    parser.add_argument("--gateway", help="Add an H9 connection alias for this callsign")
    parser.add_argument("--allow-tx", action="store_true", help="Enable RF transmission on manual connect")
    parser.add_argument("--open-browser", action="store_true", help="Open the Pat web interface when ready")
    try:
        run(parser.parse_args())
    except KeyboardInterrupt:
        pass
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as exc:
        raise SystemExit(f"H9 setup stopped: {exc}") from None


if __name__ == "__main__":
    main()
