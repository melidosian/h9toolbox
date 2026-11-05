import { KissTnc } from './tnc.mjs';
const $ = id => document.getElementById(id);
const tnc = new KissTnc();
let busy = false;
function refresh() {
  const connected = Boolean(tnc.transport);
  $('connect').disabled = busy || connected;
  $('disconnect').disabled = busy || !connected;
  $('transport').disabled = busy || connected;
  for (const id of ['service', 'write', 'notify', 'baud']) $(id).disabled = busy || connected;
  $('bleFields').hidden = $('transport').value !== 'ble';
  $('serialFields').hidden = $('transport').value !== 'serial';
  $('sample').disabled = busy || tnc.transport !== 'demo';
  $('send').disabled = busy || !connected || (tnc.transport !== 'demo' && !$('allowTransmit').checked);
}
function log(label, text) {
  const row = document.createElement('li');
  row.textContent = `${new Date().toLocaleTimeString()} ${label} ${text}`;
  $('monitor').prepend(row);
  while ($('monitor').children.length > 200) $('monitor').lastElementChild.remove();
}
async function work(fn) {
  if (busy) return;
  busy = true; $('error').textContent = ''; refresh();
  try { await fn(); } catch (error) { $('error').textContent = error.message; }
  finally { busy = false; refresh(); }
}
function uuid(id) {
  const text = $(id).value.trim().toLowerCase();
  if (/^[a-f0-9]{4}$/.test(text)) return `0000${text}-0000-1000-8000-00805f9b34fb`;
  if (!/^[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}$/.test(text)) throw new Error('Enter a full BLE UUID or a four-digit Bluetooth UUID.');
  return text;
}
tnc.addEventListener('state', event => { $('state').textContent = event.detail; $('allowTransmit').checked = false; refresh(); });
tnc.addEventListener('error', event => { $('error').textContent = event.detail; });
tnc.addEventListener('packet', ({ detail: frame }) => {
  const hex = Array.from(frame.payload, b => b.toString(16).padStart(2, '0')).join(' ');
  log(`${frame.demo ? 'DEMO ' : ''}RX port ${frame.port}${frame.command ? ` command ${frame.command}` : ''}`, frame.aprs ?? `${frame.decodeError || 'KISS control'} | ${hex}`);
});
tnc.addEventListener('sent', ({ detail: frame }) => log(`${frame.demo ? 'DEMO TX (no RF)' : 'TX → TNC'} port ${frame.port}`, frame.aprs));
$('transport').addEventListener('change', refresh);
$('allowTransmit').addEventListener('change', refresh);
$('connect').addEventListener('click', () => work(async () => {
  $('allowTransmit').checked = false;
  if ($('transport').value === 'demo') tnc.connectDemo();
  else if ($('transport').value === 'serial') await tnc.connectSerial(Number($('baud').value));
  else await tnc.connectBle({ service: uuid('service'), write: uuid('write'), notify: uuid('notify') });
}));
$('disconnect').addEventListener('click', () => work(() => tnc.disconnect()));
$('sample').addEventListener('click', () => work(() => tnc.demoReceive()));
$('send').addEventListener('click', () => work(() => tnc.sendAprs($('packet').value, { port: Number($('port').value), allowTransmit: $('allowTransmit').checked })));
$('clear').addEventListener('click', () => $('monitor').replaceChildren());
refresh();
