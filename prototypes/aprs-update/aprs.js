import { flashAprs, IMAGE_SIZE, IMAGES, isAprsRadio, RADIO_FILTER, STOCK_URL, verifyImage } from './protocol.mjs';

const ids = ['prepare', 'stockFile', 'saveStock', 'imageState', 'deviceState',
  'connect', 'target', 'confirmMode', 'flash', 'saveJournal', 'progress', 'progressText', 'status'];
const ui = Object.fromEntries(ids.map(id => [id, document.getElementById(id)]));
const serial = navigator.serial;
const images = {};
let port = null;
let busy = false;
let writing = false;
let journal = [];

function ready() {
  return Boolean(port && images.stock && images[ui.target.value]);
}

function render() {
  ui.prepare.disabled = busy;
  ui.stockFile.disabled = busy;
  ui.saveStock.disabled = busy || !images.stock;
  ui.connect.disabled = busy || !serial;
  ui.target.disabled = busy;
  ui.confirmMode.disabled = busy || !ready();
  ui.flash.disabled = busy || !ready() || !ui.confirmMode.checked;
  ui.saveJournal.disabled = busy || !journal.length;
  ui.deviceState.textContent = writing ? 'Writing via USB-C' : port ? 'USB port selected' : 'Disconnected';
  if (images.stock || images.packet) {
    ui.imageState.textContent = `${images.packet ? 'Packet fix verified.' : 'Packet fix pending.'} ${images.stock ? 'Stock recovery image verified.' : 'Stock recovery image required.'}`;
  }
}

function status(text, failed = false) {
  ui.status.textContent = text;
  ui.status.classList.toggle('failure', failed);
}

async function action(task) {
  if (busy) return;
  busy = true;
  render();
  try {
    await task();
  } catch (error) {
    if (error.name === 'NotFoundError') status('No USB port selected.');
    else status(error.message, true);
  } finally {
    busy = false;
    render();
  }
}

async function getImage(kind, url) {
  const response = await fetch(url, { signal: AbortSignal.timeout(15_000) });
  if (!response.ok) throw new Error(`Could not load ${IMAGES[kind].label} (HTTP ${response.status}).`);
  const bytes = new Uint8Array(await response.arrayBuffer());
  await verifyImage(bytes, kind);
  images[kind] = bytes;
}

function download(filename, blob) {
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.href = url;
  link.download = filename;
  link.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

function progress(bytes) {
  ui.progress.value = bytes;
  ui.progressText.textContent = `${bytes.toLocaleString()} / ${IMAGE_SIZE.toLocaleString()} bytes acknowledged`;
}

ui.prepare.addEventListener('click', () => action(async () => {
  status('Loading and checking firmware images…');
  const results = await Promise.allSettled([
    images.packet ?? getImage('packet', `./${IMAGES.packet.filename}`),
    images.stock ?? getImage('stock', STOCK_URL),
  ]);
  const failed = results.find(result => result.status === 'rejected');
  if (failed) throw new Error(`${failed.reason.message} You can choose a matching local stock file if the download is unavailable.`);
  status('Firmware verified. Save the stock image for recovery before writing.');
}));

ui.stockFile.addEventListener('change', () => action(async () => {
  const file = ui.stockFile.files[0];
  ui.stockFile.value = '';
  if (!file) return;
  if (file.size !== IMAGE_SIZE) throw new Error('The stock APRS image must be 17,304 bytes.');
  const bytes = new Uint8Array(await file.arrayBuffer());
  await verifyImage(bytes, 'stock');
  images.stock = bytes;
  status('Stock recovery image verified.');
}));

ui.saveStock.addEventListener('click', () => {
  download(IMAGES.stock.filename, new Blob([images.stock], { type: 'application/octet-stream' }));
});

ui.connect.addEventListener('click', () => action(async () => {
  const selected = await serial.requestPort({ filters: [RADIO_FILTER] });
  if (!isAprsRadio(selected)) throw new Error('Select the TD-H9 USB serial port.');
  port = selected;
  ui.confirmMode.checked = false;
  status('USB port selected. Confirm APRS Update is on the radio screen.');
}));

serial?.addEventListener('disconnect', event => {
  if ((event.port ?? event.target) === port) {
    port = null;
    ui.confirmMode.checked = false;
    if (!writing) status('The radio disconnected. Select its USB port when it is ready.');
    render();
  }
});

ui.target.addEventListener('change', () => {
  ui.confirmMode.checked = false;
  render();
});
ui.confirmMode.addEventListener('change', render);

ui.flash.addEventListener('click', () => {
  if (!ready() || !ui.confirmMode.checked) return;
  void action(async () => {
    const kind = ui.target.value;
    writing = true;
    ui.confirmMode.checked = false;
    journal = [];
    progress(0);
    render();
    status('Opening USB serial and waiting for quiet input…');
    try {
      await flashAprs(port, images[kind], kind, { onEvent(row) {
        journal.push({ utc: new Date().toISOString(), ...row });
        if (row.event === 'startup_drained') status('Writing APRS firmware. Keep the USB cable connected.');
        if (row.event === 'block_acknowledged') progress(row.acknowledged_end);
      } });
      status('Transfer acknowledged. Power-cycle the radio normally, then verify APRS Info and packet reception.');
    } catch (error) {
      const offset = ui.progress.value.toLocaleString();
      throw new Error(`Update stopped at ${offset} acknowledged bytes. ${error.message} Re-enter APRS Update before starting over or restoring stock.`);
    } finally {
      writing = false;
      port = null;
    }
  });
});

ui.saveJournal.addEventListener('click', () => {
  const lines = journal.map(row => JSON.stringify(row)).join('\n') + '\n';
  download('h9-aprs-transfer.jsonl', new Blob([lines], { type: 'application/x-ndjson' }));
});

window.addEventListener('beforeunload', event => {
  if (writing) {
    event.preventDefault();
    event.returnValue = '';
  }
});

if (!serial) status('USB-C access requires desktop Chrome or Edge over HTTPS.');
render();
