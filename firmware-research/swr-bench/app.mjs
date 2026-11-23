import { connectBluetooth } from '../../dist/boot-logo/radio.mjs';
import { ADC_IDS, captureCsv, readCapture } from './read-capture.mjs';

const read = document.getElementById('read');
const status = document.getElementById('status');
const save = document.getElementById('save');
const review = document.getElementById('review');
let result;

function showCapture(rows) {
  const before = rows.filter(row => row.relativeMs < 0);
  const transmitting = rows.filter(row => row.txFlag === 1);
  function range(samples, channel) {
    const values = samples.map(row => row.adc[channel]).filter(value => value !== null);
    if (!values.length) return '—';
    const low = Math.min(...values), high = Math.max(...values);
    return low === high ? String(low) : `${low}–${high}`;
  }

  document.getElementById('summary').textContent =
    `${rows.length} samples · ${before.length} before PTT · ${transmitting.length} during TX`;
  document.getElementById('ranges').replaceChildren(...ADC_IDS.map((id, channel) => {
    const row = document.createElement('tr');
    const label = document.createElement('th');
    label.scope = 'row';
    label.textContent = `ADC ${id}`;
    row.append(label);
    for (const samples of [before, transmitting]) {
      const cell = document.createElement('td');
      cell.textContent = range(samples, channel);
      row.append(cell);
    }
    return row;
  }));
  review.hidden = false;
}

read.addEventListener('click', async () => {
  read.disabled = true;
  save.disabled = true;
  review.hidden = true;
  status.textContent = 'Select the TD-H9.';
  result = undefined;
  let connection;
  try {
    if (!navigator.bluetooth) throw new Error('Open this page in a desktop browser with Web Bluetooth on localhost.');
    connection = await connectBluetooth();
    status.textContent = 'Checking the capture…';
    const capture = await readCapture(connection.radio, fraction => {
      status.textContent = `Reading samples · ${Math.floor(fraction * 100)}%`;
    });
    if (capture.status === 0) {
      status.textContent = 'Capture armed. With the analyzer disconnected and a rated load attached, briefly key and release PTT on Low. Then read again.';
    } else if (capture.status === 1) {
      status.textContent = 'Capture in progress. Release PTT and wait one second before reading again.';
    } else {
      result = capture;
      showCapture(capture.rows);
      save.disabled = false;
      status.textContent = capture.status === 3
        ? 'Sample limit reached. The CSV contains the available samples.'
        : 'Capture ready to download.';
    }
  } catch (error) {
    status.textContent = error.message;
  } finally {
    connection?.disconnect();
    read.disabled = false;
  }
});

save.addEventListener('click', () => {
  if (!result) return;
  const label = document.getElementById('label').value.trim().replace(/[^\p{L}\p{N}_.-]+/gu, '-');
  const stamp = new Date().toISOString().replace(/[:.]/g, '-');
  const name = ['td-h9-swr', label, result.status === 3 ? 'limited' : '', stamp].filter(Boolean).join('-');
  const url = URL.createObjectURL(new Blob([captureCsv(result.rows)], { type: 'text/csv' }));
  const link = document.createElement('a');
  link.href = url;
  link.download = `${name}.csv`;
  document.body.append(link);
  link.click();
  link.remove();
  setTimeout(() => URL.revokeObjectURL(url), 60000);
});
