// Boot-logo tool: connect over Bluetooth, read and save the current logo,
// convert any PNG/JPG (or a saved .bin) to the radio's 160 x 128 RGB565
// format, then write and verify it. Mounted by the Boot logo page and by the
// Firmware update page as its last step, and by the Bluetooth programmer on
// its own connection (the `connection` option).
import { LOGO_ADDRESS, LOGO_BYTES, LOGO_HEIGHT, LOGO_WIDTH, STOCK_LOGO_SHA256, fitRect, logoFromBin, logoSectorPlan, logoToRgba, rgbaToLogo } from './resource.mjs?v=20261108a';
import { Stopped, connectBluetooth } from './radio.mjs?v=20261109b';

const MARKUP = `
  <ol class="instructions" data-id="instructions">
    <li>Install <strong>H9 Toolbox Firmware</strong> on the radio and power it on normally.</li>
    <li>Turn on the radio’s Bluetooth programming mode, then click Connect and choose it from the browser’s device list.</li>
  </ol>
  <div class="controls" data-id="connectRow"><button class="button primary-button" data-id="connect">Connect Bluetooth</button><button class="button ghost" data-id="disconnect" disabled>Disconnect</button><span class="chip" data-id="state">Disconnected</span></div>
  <div class="logo-pair">
    <figure><canvas data-id="current" width="160" height="128"></canvas><figcaption>On the radio <span data-id="currentInfo">· not read yet</span></figcaption></figure>
    <figure><canvas data-id="next" width="160" height="128"></canvas><figcaption>New logo <span data-id="nextInfo">· choose an image</span></figcaption></figure>
  </div>
  <div class="controls"><button class="button ghost" data-id="read" disabled>Read current logo</button><button class="button ghost" data-id="save" disabled>Save current logo (.bin)</button></div>
  <div class="logo-options">
    <label>Image (PNG, JPG, or a saved logo .bin)<input data-id="file" type="file" accept="image/png,image/jpeg,image/webp,image/gif,image/bmp,.bin,application/octet-stream"></label>
    <label>Fit<select data-id="fit"><option value="contain" selected>Whole image, bars around it</option><option value="cover">Fill the screen, crop edges</option><option value="stretch">Stretch to fill</option></select></label>
    <label>Background<input data-id="background" type="color" value="#000000"></label>
    <label class="confirm"><input data-id="dither" type="checkbox" checked><span>Smooth gradients (dither; best for photos)</span></label>
    <label class="confirm"><input data-id="sharp" type="checkbox"><span>Sharp pixels (pixel art)</span></label>
  </div>
  <p class="fine">The preview shows exactly what the radio will display: 160 × 128 pixels in its 16-bit colour. Writing starts only after you save the current logo, so you can always put it back by choosing that .bin.</p>
  <div class="controls"><button class="button write-button" data-id="write" disabled>Write logo to radio</button><button class="button ghost" data-id="stop" disabled>Stop after this sector</button></div>
  <div class="progress-wrap"><div class="progress-track"><div data-id="bar"></div></div><span data-id="progressText">Idle</span></div>
  <div class="result-panel" data-id="result" role="status" hidden></div>
`;

function formatDuration(seconds) {
  const whole = Math.max(1, Math.ceil(seconds));
  if (whole < 90) return `${whole} s`;
  return `${Math.floor(whole / 60)} min ${String(whole % 60).padStart(2, '0')} s`;
}

async function digest(bytes) {
  const hash = await crypto.subtle.digest('SHA-256', bytes);
  return Array.from(new Uint8Array(hash), value => value.toString(16).padStart(2, '0')).join('');
}

// Decode any browser-supported image with its EXIF orientation, halve it
// until it is at most twice the target size (one large downscale aliases),
// then draw it onto the chosen background at the 160 x 128 fit.
async function imageToRgba(file, { fit, background, sharp }) {
  let bitmap;
  try {
    bitmap = await createImageBitmap(file, { imageOrientation: 'from-image' });
  } catch {
    throw new Error('This browser could not decode that image. Use a PNG or JPG.');
  }
  const rect = fitRect(bitmap.width, bitmap.height, fit);
  let source = bitmap, width = bitmap.width, height = bitmap.height;
  while (!sharp && (width > rect.width * 2 || height > rect.height * 2)) {
    width = Math.max(Math.ceil(width / 2), Math.ceil(rect.width));
    height = Math.max(Math.ceil(height / 2), Math.ceil(rect.height));
    const step = new OffscreenCanvas(width, height);
    const context = step.getContext('2d');
    context.imageSmoothingQuality = 'high';
    context.drawImage(source, 0, 0, width, height);
    source = step;
  }
  const canvas = new OffscreenCanvas(LOGO_WIDTH, LOGO_HEIGHT);
  const context = canvas.getContext('2d');
  context.fillStyle = background;
  context.fillRect(0, 0, LOGO_WIDTH, LOGO_HEIGHT);
  context.imageSmoothingEnabled = !sharp;
  context.imageSmoothingQuality = 'high';
  context.drawImage(source, rect.x, rect.y, rect.width, rect.height);
  bitmap.close();
  return context.getImageData(0, 0, LOGO_WIDTH, LOGO_HEIGHT).data;
}

// onStatus(message, isError) reports to the host page's activity log.
// connection: a host that already holds the Bluetooth link supplies
// { session() -> { radio, name } | null, onBusy(flag), blocked() -> bool
// (the host is using the link) }; the tool then hides
// its own Connect controls. Returns { refresh, reset } for such a host:
// refresh after the link changes, reset when a different radio connects.
export function mountLogoTool(root, { onStatus = () => {}, connection = null } = {}) {
  root.innerHTML = MARKUP;
  const el = id => root.querySelector(`[data-id="${id}"]`);
  let ownSession = null, busy = false, current = null, next = null, nextSource = null, saved = false;
  const getSession = () => (connection ? connection.session() : ownSession);
  if (connection) { el('instructions').hidden = true; el('connectRow').hidden = true; }
  const status = (message, error = false) => onStatus(message, error);

  function draw(id, bytes) {
    el(id).getContext('2d').putImageData(new ImageData(logoToRgba(bytes), LOGO_WIDTH, LOGO_HEIGHT), 0, 0);
  }
  // Mark the "On the radio" preview as checked against the radio, or not.
  function markVerified(verified) {
    el('current').classList.toggle('verified', verified);
    root.querySelector('.verified-badge')?.remove();
    if (verified) {
      const badge = document.createElement('span'); badge.className = 'verified-badge'; badge.textContent = 'Verified on radio';
      el('current').nextElementSibling.append(badge);
    }
  }
  function showResult(result) {
    const panel = el('result');
    panel.hidden = !result;
    if (!result) return;
    panel.classList.toggle('failed', Boolean(result.failed));
    const title = document.createElement('strong'); title.textContent = result.title;
    const body = document.createElement('div');
    const lead = document.createElement('p'); lead.textContent = result.text; body.append(lead);
    if (result.steps) {
      const list = document.createElement('ol');
      for (const step of result.steps) { const item = document.createElement('li'); item.textContent = step; list.append(item); }
      body.append(list);
    }
    panel.replaceChildren(title, body);
    panel.scrollIntoView({ behavior: 'smooth', block: 'center' });
  }
  function progress(fraction, text) {
    el('bar').style.width = `${Math.round(fraction * 100)}%`;
    el('progressText').textContent = text;
  }
  // Time left at the rate measured since `began`, once there is a rate.
  function timed(began) {
    return (fraction, text) => {
      const elapsed = (performance.now() - began) / 1000;
      const left = fraction > 0.02 && fraction < 1 && elapsed >= 2 ? ` · about ${formatDuration(elapsed * (1 - fraction) / fraction)} left` : '';
      progress(fraction, `${Math.floor(fraction * 100)}% · ${text}${left}`);
    };
  }
  async function describe(bytes) {
    const hash = await digest(bytes);
    return `· SHA-256 ${hash.slice(0, 12)}…${hash === STOCK_LOGO_SHA256 ? ' (stock TIDRADIO logo)' : ''}`;
  }
  function refresh() {
    const connected = Boolean(getSession()) && !(!busy && connection?.blocked?.());
    el('connect').disabled = busy || connected;
    el('disconnect').disabled = busy || !connected;
    el('read').disabled = busy || !connected;
    el('save').disabled = busy || !current;
    el('write').disabled = busy || !connected || !current || !next || !saved;
    el('stop').disabled = !busy;
    for (const id of ['file', 'fit', 'background', 'dither', 'sharp']) el(id).disabled = busy;
    el('write').title = !current ? 'Read the current logo first.' : !saved ? 'Save the current logo first.' : !next ? 'Choose a new image.' : '';
  }
  async function run(task) {
    busy = true; connection?.onBusy?.(true); refresh();
    try { await task(); } finally { busy = false; connection?.onBusy?.(false); refresh(); }
  }

  async function readCurrent() {
    const began = performance.now();
    const report = timed(began);
    const result = await getSession().radio.readRange(LOGO_ADDRESS, LOGO_BYTES, (fraction, rate) =>
      report(fraction, `Reading the current logo · ${(rate / 1024).toFixed(1)} KiB/s`));
    current = result.bytes;
    draw('current', current);
    el('currentInfo').textContent = await describe(current);
    markVerified(false);
    progress(1, `100% · Read the current logo in ${formatDuration(result.seconds)}`);
    status(`Read the current boot logo (${result.method === 'fast' ? 'fast' : '32-byte'} read, ${formatDuration(result.seconds)}). Save it before writing a new one.`);
    await prepare();
  }

  async function prepare() {
    if (!nextSource) return;
    const file = nextSource;
    if (/\.bin$/i.test(file.name)) {
      next = logoFromBin(new Uint8Array(await file.arrayBuffer()));
    } else {
      const rgba = await imageToRgba(file, { fit: el('fit').value, background: el('background').value, sharp: el('sharp').checked });
      next = rgbaToLogo(rgba, { dither: el('dither').checked });
    }
    draw('next', next);
    const plan = current ? logoSectorPlan(current, next) : null;
    el('nextInfo').textContent = `${await describe(next)}${plan ? ` · ${plan.length} of 10 sectors change` : ''}`;
  }

  el('connect').addEventListener('click', () => run(async () => {
    try {
      ownSession = await connectBluetooth({
        log: (message, level) => { if (level !== 'debug') console[level === 'warn' ? 'warn' : 'info']('[boot-logo]', message); },
        onDisconnect: () => { ownSession = null; el('state').textContent = 'Disconnected'; status('Radio disconnected.'); refresh(); },
      });
      el('state').textContent = `Connected · ${ownSession.name}`;
      saved = false;
      const service = await ownSession.radio.checkService();
      if (service.version !== 2) throw new Error('This radio’s firmware cannot write the boot logo. Install H9 Toolbox Firmware first.');
      if (!service.ready) throw new Error('The radio has not finished starting its resource flash. Power-cycle it and connect again.');
      status(`Connected to ${ownSession.name}; the boot-logo service answered.`);
      await readCurrent();
    } catch (error) {
      if (error.name === 'NotFoundError') { status('No radio chosen.'); return; }
      ownSession?.disconnect(); ownSession = null; el('state').textContent = 'Disconnected';
      status(/No resource service/.test(error.message) ? 'This radio’s firmware has no boot-logo service. Install H9 Toolbox Firmware first.' : error.message, true);
    }
  }));
  el('disconnect').addEventListener('click', () => { ownSession?.disconnect(); ownSession = null; el('state').textContent = 'Disconnected'; refresh(); });
  el('read').addEventListener('click', () => run(async () => {
    try { await readCurrent(); } catch (error) { progress(0, 'Read stopped'); status(`Logo read failed: ${error.message}`, true); }
  }));
  el('save').addEventListener('click', async () => {
    const url = URL.createObjectURL(new Blob([current], { type: 'application/octet-stream' }));
    const link = document.createElement('a');
    link.href = url; link.download = `td-h9-boot-logo-${(await digest(current)).slice(0, 8)}.bin`;
    document.body.append(link); link.click(); link.remove();
    setTimeout(() => URL.revokeObjectURL(url), 60000);
    saved = true;
    status('Current logo download started. Keep that .bin: choosing it as the new image puts this logo back.');
    refresh();
  });
  const choose = () => run(async () => {
    nextSource = el('file').files[0] ?? null;
    if (!nextSource) { next = null; return; }
    try { await prepare(); status(`Prepared ${nextSource.name} for the radio.`); }
    catch (error) { next = null; el('nextInfo').textContent = '· not usable'; status(error.message, true); }
  });
  for (const id of ['file', 'fit', 'background', 'dither', 'sharp']) el(id).addEventListener('change', choose);
  el('stop').addEventListener('click', () => { const active = getSession(); if (active) active.radio.stopRequested = true; el('stop').disabled = true; });
  el('write').addEventListener('click', () => run(async () => {
    const began = performance.now();
    const session = getSession();
    session.radio.stopRequested = false;
    showResult(null);
    try {
      const result = await session.radio.writeLogo(current, next, timed(began));
      draw('current', current);
      el('currentInfo').textContent = await describe(current);
      el('nextInfo').textContent = `${await describe(next)} · 0 of 10 sectors change`;
      const seconds = formatDuration((performance.now() - began) / 1000);
      progress(1, `100% · ${result.sectors ? `${result.sectors} sectors written and the whole logo verified` : 'The radio already shows this logo'} in ${seconds}`);
      markVerified(true);
      showResult(result.sectors
        ? { title: 'Boot logo written and verified',
            text: `${result.sectors} of 10 logo sectors were rewritten, then all 40,960 bytes were read back from the radio and match the new logo exactly (${seconds}).`,
            steps: ['Power-cycle the radio: the new logo appears at startup.', 'To go back, choose the .bin you saved and write again.'] }
        : { title: 'The radio already shows this logo', text: 'Every byte on the radio already matches; nothing was written.' });
      status(result.sectors ? 'Boot logo written and verified. Power-cycle the radio to see it.' : 'The radio already holds this logo; nothing was written.');
    } catch (error) {
      el('progressText').textContent = `Stopped: ${error.message}`;
      if (error instanceof Stopped) {
        // Stopped between sectors: `current` matches the radio exactly.
        draw('current', current);
        el('nextInfo').textContent = `${await describe(next)} · ${logoSectorPlan(current, next).length} of 10 sectors still change`;
        status('Stopped between sectors. The radio shows part of each logo; click Write again to finish, or choose your saved .bin to go back.');
        showResult({ failed: true, title: 'Logo write stopped', text: 'Stopped cleanly between sectors, so the radio now shows part of each logo.',
          steps: ['Click Write logo to radio to finish.', 'Or choose the .bin you saved to go back.'] });
      } else {
        // A sector may be half done: re-read before planning again.
        current = null;
        el('currentInfo').textContent = '· read again before resuming';
        status(`Logo write stopped: ${error.message} Click Read current logo, then Write to resume (or choose your saved .bin to go back).`, true);
        showResult({ failed: true, title: 'Logo write did not finish', text: error.message,
          steps: ['Keep the radio in Bluetooth programming mode (reconnect if needed).', 'Click Read current logo, then Write logo to radio to resume.', 'Or choose the .bin you saved to go back.'] });
      }
    }
  }));
  refresh();
  return {
    refresh,
    // A different radio: forget its logo and require a fresh save.
    reset() { current = null; saved = false; markVerified(false); el('currentInfo').textContent = '· not read yet'; showResult(null); refresh(); },
  };
}
