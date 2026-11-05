#!/usr/bin/env python3
"""Serve this standalone prototype locally and open it in the default browser."""
import argparse
import errno
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import webbrowser


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=8001)
    parser.add_argument('--no-browser', action='store_true')
    args = parser.parse_args()
    handler = partial(SimpleHTTPRequestHandler, directory=str(Path(__file__).resolve().parent))
    try:
        server = ThreadingHTTPServer(('127.0.0.1', args.port), handler)
    except OSError as error:
        if error.errno != errno.EADDRINUSE:
            raise
        server = ThreadingHTTPServer(('127.0.0.1', 0), handler)
        print(f'Port {args.port} is in use; selected an available port.', flush=True)
    with server:
        url = f'http://127.0.0.1:{server.server_port}/'
        print(f'Browser KISS TNC: {url}', flush=True)
        print('Run this on the same computer as your browser and radio. Press Ctrl+C to stop.', flush=True)
        if not args.no_browser:
            webbrowser.open(url)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == '__main__':
    main()
