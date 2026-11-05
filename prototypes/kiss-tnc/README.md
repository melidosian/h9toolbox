# Browser KISS TNC proof of concept

Standalone, buildless transport layer for a future web APRS client. The GitHub Pages workflow copies the five web assets from this directory to `dist/kiss-tnc/` during deployment. The test page is at https://h9toolbox.com/kiss-tnc/, unlinked from the programmer and excluded from indexing with `noindex, nofollow`. It is publicly accessible to anyone with the URL. The local launcher and this README are not copied into the deployed site. Nothing here changes the existing radio programmer.

Open the test page in desktop Chrome or Edge and connect in Demo mode to try it without a radio. No local server is required for the hosted page. Hardware connections use Bluetooth or serial on the computer running your browser.

## Run locally

Run this on **your own computer**, not the `/workspace` cloud terminal. For a standalone copy, extract `h9-kiss-tnc.zip`, open a terminal in the extracted `kiss-tnc` folder, and run:

```sh
python3 start.py
```

On Windows, use `py -3 start.py`. The launcher opens your browser and selects another port if 8001 is already occupied. Keep the terminal running while using the app.

For a full repository checkout, you can also start it manually:

From the repository root, with Python 3 and desktop Chrome or Edge:

```sh
python3 -m http.server 8001 --bind 127.0.0.1 --directory prototypes/kiss-tnc
```

Open `http://localhost:8001` in the browser on that computer. Demo mode needs no radio: Connect shows a simulated received position packet; Send packet records a simulated outgoing packet without RF. Simulate received packet injects another packet through the streaming decoder.

Hardware browser APIs require localhost or HTTPS. A cloud machine's loopback address is only reachable inside that machine; run the command on your computer for access to your local radio.

## Hardware transports

- **BLE GATT:** enter the TNC's service UUID, writable characteristic UUID, and notification/indication characteristic UUID. Four-digit Bluetooth UUIDs and full UUIDs are accepted. Writes use sequential 20-byte chunks; incoming notifications may contain arbitrary packet fragments.
- **Serial:** select a KISS serial device using the browser chooser and set its configured baud rate. A Bluetooth Classic SPP device may be available after operating-system pairing, depending on browser/platform support. Web Bluetooth itself supports BLE GATT, not Classic SPP. No Bluetooth pairing or serial-mode negotiation is performed by this page.

Use a device already exposing a **standard KISS byte stream**. Connecting only subscribes to data; it sends no KISS configuration, mode-switch, programming, or transmit commands. To send to hardware, enter your own licensed callsign, enable transmission, and click Send packet. TX means the frame was handed to the transport, not that it was transmitted over RF or acknowledged by another station. There are no automatic beacons, retries, or APRS message acknowledgements.

### H9 status

The H9's internal processor/modem protocol has KISS-style framing, but its Bluetooth packet endpoint and any mode-switch procedure remain unverified. The FF00/FF01/FF02 BLE programming endpoint is **not** assumed to be a KISS endpoint. We do not send programming handshakes, probe writes, or vendor commands. Successful BLE connection does not prove KISS compatibility. A verified KISS notification capture plus the corresponding writable endpoint is the next hardware integration input.

## Reuse from a web APRS client

`tnc.mjs` provides the transport independently of this page:

```js
import { KissTnc } from './tnc.mjs';
const tnc = new KissTnc();
tnc.addEventListener('packet', ({ detail }) => {
  // detail: { port, command, payload: Uint8Array, aprs, decodeError, demo }
  // aprs is monitor text for AX.25 UI (03/F0) frames, otherwise null.
  console.log(detail.aprs, detail.payload);
});
tnc.addEventListener('state', ({ detail }) => console.log(detail));
tnc.addEventListener('error', ({ detail }) => console.error(detail));
tnc.addEventListener('sent', ({ detail }) => console.log('To TNC:', detail));

// No hardware needed:
tnc.connectDemo();
await tnc.sendAprs('N0CALL-1>APRS:>Demo packet', { port: 0 });
await tnc.disconnect();

// Hardware connection methods, invoked from a user gesture:
// await tnc.connectBle({ service, write, notify });
// await tnc.connectSerial(9600);
// await tnc.sendAprs('YOURCALL>APRS:>Hello', { port: 0, allowTransmit: true });
```

`kiss.mjs` exports the reusable streaming `KissDecoder`, `encodeKiss`, `encodeAprs`, and `decodeAprs`. KISS framing supports ports 0–15, escaping, fragmented/coalesced input, malformed-frame recovery, and a 4096-byte frame bound. APRS transmission supports AX.25 UI frames with printable ASCII information (1–256 bytes), SSIDs, and up to eight digipeaters. KISS packets omit the RF FCS; the TNC handles it. Other command frames and non-UI data remain visible as hex.

This is a packet monitor/composer, not a complete APRS application: no map, GPS, APRS-IS, persistent station database, message retries, or decoding of individual APRS information-field formats. Packet text is rendered as text, never HTML. The monitor retains the latest 200 entries in memory. No analytics, external dependencies, uploads, or browser storage are used by the application.

## Verify

```sh
node --test test/kiss-tnc.test.mjs
node --test test/*.test.mjs
```

The tests cover independent KISS/AX.25 byte vectors, split escapes, combined packets, invalid/oversized frames, address validation, demo events, and mocked BLE/serial transport I/O. Mocked hardware checks verify passive connections, explicit TX permission, BLE chunking, notification byte offsets, and cleanup. Hardware RF operation and H9 compatibility still require a physical device.
