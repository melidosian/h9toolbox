import { connectBluetooth } from '../../dist/boot-logo/radio.mjs?v=up2';
import { SLOTS, planLayout, restore, uploadPack } from './upload.mjs?v=up3';

const $ = id => document.getElementById(id);
let session = null, busy = false, pack = null, manifest = null;

function log(message, error = false) {
  $('log').textContent = `${new Date().toLocaleTimeString()} ${message}\n` + $('log').textContent;
  if (error) console.error('[data-upload]', message); else console.info('[data-upload]', message);
}
function refresh() {
  const connected = Boolean(session);
  $('connect').disabled = busy || connected;
  $('disconnect').disabled = busy || !connected;
  $('upload').disabled = busy || !connected || !pack;
  $('stop').disabled = !busy;
  $('restore').disabled = busy || !connected;
  $('deep').disabled = busy || !connected;
  $('saveManifest').disabled = !manifest;
  $('state').textContent = connected ? `Connected · ${session.name}` : 'Disconnected';
}
async function run(task) {
  if (busy) return;
  busy = true; refresh();
  try { await task(); } catch (error) { log(error.message, true); $('result').textContent = `Stopped: ${error.message}`; } finally { busy = false; if (session) session.radio.stopRequested = false; refresh(); }
}
function progress(began) {
  return (fraction, phase, rate) => {
    $('progress').value = fraction;
    const elapsed = (performance.now() - began) / 1000;
    const left = fraction > 0.03 ? ` · about ${Math.ceil(elapsed * (1 - fraction) / fraction / 60)} min left` : '';
    $('progressText').textContent = `${Math.floor(fraction * 100)}% · ${phase}${rate ? ` · writing ${(rate / 1024).toFixed(2)} KiB/s` : ''}${left}`;
  };
}

$('file').addEventListener('change', async () => {
  const file = $('file').files[0];
  pack = file ? new Uint8Array(await file.arrayBuffer()) : null;
  $('plan').replaceChildren();
  if (pack) {
    try {
      const slot = SLOTS[$('slot').value];
      const rows = [['Area', 'Chip address', 'Bytes'], [`${slot.name} (directory)`, `0x${slot.directory.toString(16)}`, '4,096'],
        ...planLayout(pack.length, slot).map(s => [s.area, `0x${s.address.toString(16)}`, s.length.toLocaleString()])];
      $('plan').replaceChildren(...rows.map((cells, i) => {
        const row = document.createElement('tr');
        for (const text of cells) { const cell = document.createElement(i ? 'td' : 'th'); cell.textContent = text; row.append(cell); }
        return row;
      }));
      log(`Selected ${file.name}: ${pack.length.toLocaleString()} bytes.`);
    } catch (error) { log(error.message, true); pack = null; }
  }
  refresh();
});
$('slot').addEventListener('change', () => $('file').dispatchEvent(new Event('change')));
$('connect').addEventListener('click', () => run(async () => {
  session = await connectBluetooth({
    log: (message, level) => { if (level !== 'debug') log(message, level === 'error'); },
    onDisconnect: () => { session = null; log('Radio disconnected.'); refresh(); },
  });
  const service = await session.radio.checkService();
  if (service.version !== 2) throw new Error('This radio does not run H9 Toolbox Firmware (write service missing).');
  log(`Connected: write service ready (${service.ready ? 'flash ready' : 'flash not ready'}).`);
}));
$('disconnect').addEventListener('click', () => { session?.disconnect(); session = null; refresh(); });
$('stop').addEventListener('click', () => { if (session) session.radio.stopRequested = true; });
$('upload').addEventListener('click', () => run(async () => {
  manifest = null;
  $('result').textContent = '';
  const name = $('name').value.trim() || 'pack';
  manifest = await uploadPack(session.radio, pack, { name, slot: SLOTS[$('slot').value], onProgress: progress(performance.now()) });
  manifest.uploaded = new Date().toISOString();
  const rate = (manifest.writeBytesPerSecond / 1024).toFixed(2);
  $('result').innerHTML = `<span class="ok">Uploaded and verified.</span> ${manifest.bytes.toLocaleString()} bytes; wrote ${manifest.sectorsWritten} sectors in ${(manifest.writeSeconds / 60).toFixed(1)} min (${rate} KiB/s); ${(manifest.totalSeconds / 60).toFixed(1)} min in all.`;
  log(`Upload complete: ${JSON.stringify(manifest)}`);
}));
$('saveManifest').addEventListener('click', () => {
  const blob = new Blob([JSON.stringify(manifest, null, 2)]);
  const link = document.createElement('a');
  link.href = URL.createObjectURL(blob);
  link.download = `td-h9-upload-${new Date().toISOString().slice(0, 19).replaceAll(':', '')}.json`;
  document.body.append(link); link.click(); link.remove();
});
for (const [id, deep] of [['restore', false], ['deep', true]]) {
  $(id).addEventListener('click', () => run(async () => {
    const result = await restore(session.radio, { deep, slot: SLOTS[$('slot').value], onProgress: progress(performance.now()) });
    $('result').textContent = result.note ?? `Restored: erased ${result.erased} sector(s)${result.name ? ` of "${result.name}"` : ''}; read back as erased.`;
    log($('result').textContent);
  }));
}
refresh();
