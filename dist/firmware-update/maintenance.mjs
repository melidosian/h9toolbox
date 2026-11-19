import { parseBundle } from '../apps/bundle.mjs?v=am3';
import { connectUsb, grantedUsbPorts } from '../boot-logo/radio.mjs?v=usb4';
import { SLOTS, readPack } from '../doom/upload.mjs?v=usb1';

export async function rebootFromHid(hid, { timeoutMs = 6000 } = {}) {
  const usbHid = navigator.hid;
  let onDisconnect, timer, sendError;
  const disconnected = new Promise(resolve => {
    onDisconnect = event => { if (event.device === hid.device) resolve(true); };
    usbHid.addEventListener('disconnect', onDisconnect);
    timer = setTimeout(() => resolve(false), timeoutMs);
  });
  // A report can race the USB disconnect. Treat the matching disconnect as
  // the signal to look for the normal-mode port, even if sendReport rejects.
  try {
    void hid.reboot().catch(error => { sendError = error; });
    if (await disconnected) return true;
    if (sendError) throw sendError;
    return false;
  } finally {
    clearTimeout(timer);
    usbHid.removeEventListener('disconnect', onDisconnect);
  }
}

export async function inspectFirstBoot(existingPorts, { onProgress, timeoutMs = 10000 } = {}) {
  if (!navigator.serial) throw new Error('Web Serial is unavailable in this browser.');
  const deadline = performance.now() + timeoutMs;
  do {
    const ports = (await grantedUsbPorts()).filter(port => !existingPorts.includes(port));
    if (ports.length > 1) throw new Error('More than one authorized TD-H9 USB serial port appeared.');
    if (ports.length === 1) {
      const session = await connectUsb({ port: ports[0] });
      try {
        const service = await session.radio.checkService();
        if (service.version !== 2 || !service.ready) throw new Error('The Toolbox resource service is unavailable or still starting.');
        const { dir, pack } = await readPack(session.radio, SLOTS.modules, onProgress);
        if (dir && !pack) throw new Error('The app bundle on the radio is incomplete.');
        return pack ? parseBundle(pack) : [];
      } finally {
        await session.disconnect();
      }
    }
    const remaining = deadline - performance.now();
    if (remaining <= 0) break;
    await new Promise(resolve => setTimeout(resolve, Math.min(remaining, 250)));
  } while (performance.now() <= deadline);
  throw new Error('An authorized TD-H9 USB serial port did not appear.');
}
