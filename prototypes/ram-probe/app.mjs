import { connectBluetooth } from '../../dist/boot-logo/radio.mjs?v=ram1';
import { RAM_SIZE } from '../../dist/boot-logo/resource.mjs?v=ram1';
import { HEAP_READ_LENGTH, HEAP_READ_START, parseHeap } from './heap.mjs';

const $ = id => document.getElementById(id);
let session = null, busy = false, watchTimer = null;
const history = [];

function log(message, error = false) {
  $('log').textContent = `${new Date().toLocaleTimeString()} ${message}\n` + $('log').textContent;
  if (error) console.error('[ram-probe]', message); else console.info('[ram-probe]', message);
}
function refresh() {
  const connected = Boolean(session);
  $('connect').disabled = busy || connected;
  $('disconnect').disabled = busy || !connected;
  $('sample').disabled = busy || !connected;
  $('watch').disabled = !connected;
  $('dump').disabled = busy || !connected;
  $('saveHistory').disabled = !history.length;
  $('state').textContent = connected ? `Connected · ${session.name}` : 'Disconnected';
}
async function run(task) {
  if (busy) return;
  busy = true; refresh();
  try { await task(); } catch (error) { log(error.message, true); } finally { busy = false; refresh(); }
}
function save(blob, name) {
  const url = URL.createObjectURL(blob), link = document.createElement('a');
  link.href = url; link.download = name; document.body.append(link); link.click(); link.remove();
  setTimeout(() => URL.revokeObjectURL(url), 60000);
}
function showHeap(heap) {
  $('map').replaceChildren(...heap.blocks.map(block => { const cell = document.createElement('i'); if (!block.free) cell.className = 'used'; cell.title = `0x${block.address.toString(16)} ${block.free ? 'free' : 'used'}`; return cell; }));
  const minFree = Math.min(...history.map(entry => entry.freeBytes));
  $('summary').textContent = `${heap.used} of ${heap.total} blocks used · ${(heap.freeBytes / 1024).toFixed(1)} KiB free · lowest free seen ${(minFree / 1024).toFixed(1)} KiB over ${history.length} reading(s)`;
  $('history').replaceChildren(...history.map(entry => {
    const row = document.createElement('tr');
    if (entry.freeBytes === minFree) row.className = 'low';
    for (const text of [entry.time, entry.label, `${entry.used} / ${entry.total}`, `${(entry.freeBytes / 1024).toFixed(1)} KiB`, Object.values(entry.checks).every(Boolean) ? 'ok' : JSON.stringify(entry.checks)]) {
      const cell = document.createElement('td'); cell.textContent = text; row.append(cell);
    }
    return row;
  }).reverse());
}
async function sampleHeap() {
  const result = await session.radio.readRam(HEAP_READ_START, HEAP_READ_LENGTH);
  const heap = parseHeap(result.bytes);
  const entry = { time: new Date().toLocaleTimeString(), at: new Date().toISOString(), label: $('label').value, used: heap.used, total: heap.total, freeBytes: heap.freeBytes, checks: heap.checks, seconds: result.seconds };
  history.push(entry);
  showHeap(heap);
  log(`Heap (${entry.label}): ${heap.used}/${heap.total} blocks used, ${(heap.freeBytes / 1024).toFixed(1)} KiB free; checks ${JSON.stringify(heap.checks)} (${result.seconds.toFixed(1)} s)`);
}

$('connect').addEventListener('click', () => run(async () => {
  session = await connectBluetooth({ log: (message, level) => { if (level !== 'debug') console.info('[radio]', message); }, onDisconnect: () => { session = null; $('watch').checked = false; clearInterval(watchTimer); log('Radio disconnected.'); refresh(); } });
  const service = await session.radio.checkService();
  log(`Connected: service version ${service.version}, flash ready ${service.ready}.`);
}));
$('disconnect').addEventListener('click', () => { session?.disconnect(); session = null; refresh(); });
$('sample').addEventListener('click', () => run(sampleHeap));
$('watch').addEventListener('change', () => {
  clearInterval(watchTimer);
  if ($('watch').checked) watchTimer = setInterval(() => { if (!busy && session) run(sampleHeap); }, 15000);
});
$('saveHistory').addEventListener('click', () => save(new Blob([JSON.stringify({ kind: 'td-h9-heap-history', readings: history }, null, 2)]), `td-h9-heap-${new Date().toISOString().slice(0, 19)}.json`));
$('dump').addEventListener('click', () => run(async () => {
  const began = performance.now();
  const result = await session.radio.readRam(0, RAM_SIZE, (fraction, rate) => {
    $('progress').value = fraction;
    const elapsed = (performance.now() - began) / 1000;
    $('progressText').textContent = `${Math.floor(fraction * 100)}% · ${(rate / 1024).toFixed(1)} KiB/s${fraction > 0.02 ? ` · about ${Math.ceil(elapsed * (1 - fraction) / fraction)} s left` : ''}`;
  });
  const heap = parseHeap(result.bytes.subarray(HEAP_READ_START));
  const stamp = new Date().toISOString().slice(0, 19).replaceAll(':', '');
  save(new Blob([result.bytes]), `td-h9-ram-${stamp}.bin`);
  save(new Blob([JSON.stringify({ kind: 'td-h9-ram-dump', label: $('label').value, captured: new Date().toISOString(), length: result.bytes.length, seconds: result.seconds, heap: { used: heap.used, total: heap.total, freeBytes: heap.freeBytes, checks: heap.checks } }, null, 2)]), `td-h9-ram-${stamp}.json`);
  $('progressText').textContent = `100% · RAM dumped in ${result.seconds.toFixed(1)} s`;
  log(`RAM dump saved (${result.bytes.length} bytes, ${result.seconds.toFixed(1)} s).`);
}));
refresh();
