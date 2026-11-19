import { KissTnc } from './tnc.mjs';
import { Ax25Link } from './ax25-link.mjs';
import { AX25, decodeAx25 } from './ax25.mjs';
const $ = id => document.getElementById(id);
const tnc = new KissTnc();
let busy = false;
let link = null;
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
  $('linkConnect').disabled = busy || !connected || tnc.transport === 'demo' || !!link && link.state !== 'closed' || !$('allowTransmit').checked;
  $('linkDisconnect').disabled = busy || !link || !['opening', 'open'].includes(link.state);
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
  const text = ($(id)?.value ?? id).trim().toLowerCase();
  if (/^[a-f0-9]{4}$/.test(text)) return `0000${text}-0000-1000-8000-00805f9b34fb`;
  if (!/^[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}$/.test(text)) throw new Error('Enter a full BLE UUID or a four-digit Bluetooth UUID.');
  return text;
}
tnc.addEventListener('state', event => {
  $('state').textContent = event.detail;
  if (event.detail === 'Disconnected' && link) { link.finish('Radio disconnected.'); link = null; }
  $('allowTransmit').checked = false; refresh();
});
tnc.addEventListener('error', event => { $('error').textContent = event.detail; });
tnc.addEventListener('packet', ({ detail: frame }) => {
  const hex = Array.from(frame.payload, b => b.toString(16).padStart(2, '0')).join(' ');
  let label = frame.aprs;
  if (!label && frame.command === 0) {
    try {
      const ax = decodeAx25(frame.payload);
      label = `${ax.source} → ${ax.destination} ${ax.kind === AX25.I ? `I ${ax.ns}/${ax.nr}` : `control 0x${ax.control.toString(16)}`} ${ax.data.length ? `| ${Array.from(ax.data, b => b >= 32 && b < 127 ? String.fromCharCode(b) : '.').join('')}` : ''}`;
    } catch { /* Leave unknown frames in hex. */ }
  }
  log(`${frame.demo ? 'DEMO ' : ''}RX port ${frame.port}${frame.command ? ` command ${frame.command}` : ''}`, label ?? `${frame.decodeError || 'KISS control'} | ${hex}`);
});
tnc.addEventListener('sent', ({ detail: frame }) => log(`${frame.demo ? 'DEMO TX (no RF)' : 'TX → TNC'} port ${frame.port}`, frame.aprs));
$('transport').addEventListener('change', refresh);
$('allowTransmit').addEventListener('change', refresh);
$('connect').addEventListener('click', () => work(async () => {
  $('allowTransmit').checked = false;
  if ($('transport').value === 'demo') tnc.connectDemo();
  else if ($('transport').value === 'serial') await tnc.connectSerial(Number($('baud').value));
  else if ($('transport').value === 'h9') await tnc.connectBle({ service: uuid('af00'), write: uuid('af01'), notify: uuid('af02'), singleWrite: true, maxFrameSize: 182, txGapMs: 1600, rxGuardMs: 700 });
  else await tnc.connectBle({ service: uuid('service'), write: uuid('write'), notify: uuid('notify') });
}));
$('disconnect').addEventListener('click', () => work(() => tnc.disconnect()));
$('sample').addEventListener('click', () => work(() => tnc.demoReceive()));
$('send').addEventListener('click', () => work(() => tnc.sendAprs($('packet').value, { port: Number($('port').value), allowTransmit: $('allowTransmit').checked })));
$('clear').addEventListener('click', () => $('monitor').replaceChildren());
function viaCalls() { return $('via').value.split(',').map(value => value.trim()).filter(Boolean); }
$('linkConnect').addEventListener('click', () => work(async () => {
  link = new Ax25Link(tnc, { local: $('localCall').value, remote: $('gatewayCall').value, via: viaCalls(), allowTransmit: $('allowTransmit').checked });
  link.addEventListener('state', event => { $('linkState').textContent = `AX.25: ${event.detail}`; refresh(); });
  link.addEventListener('tx', event => log('AX.25 TX', `0x${event.detail.frame.control.toString(16).padStart(2, '0')}`));
  link.addEventListener('data', event => log('Gateway data', `${event.detail.data.length} bytes`));
  link.addEventListener('closed', event => { log('AX.25', event.detail); refresh(); });
  link.start();
}));
$('linkDisconnect').addEventListener('click', () => link?.disconnect());
refresh();
