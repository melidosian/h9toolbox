// The App Manager's info dialog: an app's description, how to use it, notes, and (for SAT PASS) the
// satellite settings: which FM satellites to track and a button that fetches fresh orbital elements
// from CelesTrak in the browser, builds the ".SAT DATA" module and stages it for Write to radio.
import { crc16 } from '../boot-logo/resource.mjs';
import { MAX_SATELLITES, MODULE_NAME, describe, fetchTles, pack } from './satdata.mjs?v=am1';

const STORE_KEY = 'h9.sat.selected';
const el = (tag, className, text) => {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text !== undefined) node.textContent = text;
  return node;
};
const dataModule = body => ({ name: MODULE_NAME, body, bss: 0, entry: 0, crc: crc16(body) });
const list = (items, className = '') => {
  const ul = el('ul', className);
  for (const text of items) ul.append(el('li', '', text));
  return ul;
};

function savedSelection(satellites) {
  try {
    const saved = JSON.parse(localStorage.getItem(STORE_KEY));
    if (Array.isArray(saved)) return new Set(saved.filter(id => satellites.some(s => s.id === id)));
  } catch { /* storage can be blocked */ }
  return new Set(satellites.map(s => s.id));
}
function saveSelection(selected) {
  try { localStorage.setItem(STORE_KEY, JSON.stringify([...selected])); } catch { /* ignore */ }
}

export const ageText = newest => {
  const days = Math.floor((Date.now() / 1000 - newest) / 86400);
  return days <= 0 ? 'today' : `${days} day${days === 1 ? '' : 's'} old`;
};

// `ctx`: { dialog, catalog(), staged(), installed(), stage(modules|fn), downloaded(item), log(message, level), busy() }
export function mountInfo(ctx) {
  const dialog = ctx.dialog;
  let satellites = null;   // satellites.json, loaded the first time it is needed

  async function loadSatellites() {
    if (satellites) return satellites;
    const response = await fetch(new URL('satellites.json', ctx.catalog().base), { cache: 'no-store' });
    if (!response.ok) throw new Error(`The satellite list did not load (${response.status}).`);
    satellites = (await response.json()).satellites;
    return satellites;
  }

  function satelliteSection(item) {
    const section = el('section', 'info-config');
    section.append(el('h3', '', 'Satellite settings'));
    section.append(el('p', 'note', 'Choose the FM satellites SAT PASS predicts and tracks. Update fetches the latest orbital elements from CelesTrak in your browser, builds the satellite data and stages it (adding the app if needed); then Write to radio. Adding or updating the app from its card does this for you with the satellites ticked here (all, until you change them). Elements age: refresh them every week or two.'));
    const status = el('p', 'info-status');
    const rows = el('div', 'sat-list');
    const message = el('p', 'note info-message');
    const buttons = el('p');
    const update = el('button', 'primary', 'Update data and stage');
    const all = el('button', 'small', 'All'), none = el('button', 'small', 'None');
    buttons.append(update);
    section.append(status, el('div', 'sat-tools', ''), rows, message, buttons);
    section.querySelector('.sat-tools').append(all, none);

    const present = () => ctx.staged().slice(1).some(m => m.name === item.name);
    function showStatus() {
      const data = [...ctx.staged()].slice(1).find(m => m.name === MODULE_NAME);
      const here = [...ctx.installed()].slice(1).find(m => m.name === MODULE_NAME);
      const text = module => { const d = describe(module?.body); return d ? `elements of ${d.date} (${ageText(d.newest)}), ${d.count} satellite${d.count === 1 ? '' : 's'}: ${d.labels.join(', ')}` : null; };
      const shown = text(data);
      status.textContent = shown ? `Staged data: ${shown}.${here && data !== here ? ` On the radio: ${text(here) ?? 'none'}.` : ''}`
        : (text(here) ? `On the radio: ${text(here)}.` : 'Not installed yet: Add app with fresh data installs it with the latest orbital elements.');
      update.disabled = ctx.busy();
      update.textContent = present() ? 'Update data and stage' : 'Add app with fresh data';
    }
    loadSatellites().then(sats => {
      const selected = savedSelection(sats);
      const boxes = sats.map(s => {
        const label = el('label', 'sat-row');
        const box = el('input'); box.type = 'checkbox'; box.checked = selected.has(s.id);
        const text = el('span', 'sat-name', `${s.label}`);
        const detail = el('span', 'note', ` ${s.name} · ${s.up ? `up ${s.up.toFixed(3)} / ` : ''}down ${s.down.toFixed(3)} MHz${s.up ? '' : ' (receive only)'} · ${s.tone ? `tone ${s.tone.toFixed(1)} Hz` : 'no tone'}${s.note ? ` · ${s.note}` : ''}`);
        label.append(box, text, detail);
        box.addEventListener('change', () => { box.checked ? selected.add(s.id) : selected.delete(s.id); saveSelection(selected); });
        rows.append(label);
        return { s, box };
      });
      const setAll = on => { boxes.forEach(b => { b.box.checked = on; on ? selected.add(b.s.id) : selected.delete(b.s.id); }); saveSelection(selected); };
      all.addEventListener('click', () => setAll(true));
      none.addEventListener('click', () => setAll(false));
      update.addEventListener('click', async () => {
        const chosen = sats.filter(s => selected.has(s.id));
        if (!chosen.length) { message.textContent = 'Select at least one satellite.'; return; }
        if (chosen.length > MAX_SATELLITES) { message.textContent = `At most ${MAX_SATELLITES} satellites.`; return; }
        update.disabled = true; message.textContent = 'Fetching orbital elements from CelesTrak…';
        try {
          const tles = await fetchTles();
          const { body, info } = pack(tles, chosen);
          const module = dataModule(body);
          if (present()) ctx.stage([module], item); else await ctx.addApp(item, module);
          message.textContent = `Staged: elements of ${info.date}, ${info.count} satellite${info.count === 1 ? '' : 's'} (${chosen.map(s => s.label).join(', ')}). Close this and press Write to radio.`;
          ctx.log(`Staged satellite data: elements of ${info.date}, ${chosen.map(s => s.label).join(', ')}.`);
        } catch (error) { message.textContent = error.message; ctx.log(error.message, 'error'); }
        showStatus();
      });
      showStatus();
    }).catch(error => { message.textContent = error.message; });
    section.refresh = showStatus;
    return section;
  }

  // Fresh satellite data for a card's Add / Update: the satellites ticked in the dialog (all if it was
  // never opened), elements straight from CelesTrak. null (with a log line) when that fails: the caller
  // then keeps the data it already has (the app's built-in data on a first install).
  async function fresh(item) {
    try {
      const sats = await loadSatellites();
      const chosen = sats.filter(s => savedSelection(sats).has(s.id));
      if (!chosen.length) return null;
      const { body, info } = pack(await fetchTles(), chosen);
      ctx.log(`Satellite data: elements of ${info.date} for ${chosen.map(s => s.label).join(', ')}.`);
      return dataModule(body);
    } catch (error) {
      ctx.log(`Satellite data was not refreshed (${error.message}); using what the app already has.`, 'error');
      return null;
    }
  }

  function open(item) {
    dialog.replaceChildren();
    const head = el('div', 'info-head');
    head.append(el('h2', '', item.title), el('span', 'tag', `${item.category ?? 'App'} · v${item.version}`));
    const close = el('button', '', 'Close');
    close.addEventListener('click', () => dialog.close());
    head.append(close);
    dialog.append(head, el('p', 'info-lead', item.summary ?? item.description));
    if (item.screenshots?.length) {
      const strip = el('div', 'info-shots');
      for (const shot of item.screenshots) {
        const figure = el('figure');
        const img = el('img'); img.src = new URL(shot.file, ctx.catalog().base); img.alt = shot.caption; img.loading = 'lazy';
        img.width = 240; img.height = 192;
        figure.append(img, el('figcaption', '', shot.caption));
        strip.append(figure);
      }
      dialog.append(strip, el('p', 'note', 'Screens drawn by each app\'s own code from sample data (a simulation, not a photo of a radio).'));
    }
    if (item.description && item.description !== item.summary) dialog.append(el('p', '', item.description));
    if (item.controls?.length) { dialog.append(el('h3', '', 'How to use it'), list(item.controls, 'info-list')); }
    if (item.notes?.length) { dialog.append(el('h3', '', 'Good to know'), list(item.notes, 'info-list')); }
    const parts = item.modules?.length ?? 1;
    dialog.append(el('p', 'note', `${parts > 1 ? `${parts} modules, ` : ''}${item.bytes.toLocaleString()} B on the chip · ${item.ram.toLocaleString()} B RAM${parts > 1 ? ' (largest)' : ''} · menu label "${item.name}"`));
    if (item.config === 'satellites') dialog.append(satelliteSection(item));
    if (!dialog.open) dialog.showModal();
  }

  dialog.addEventListener('click', event => { if (event.target === dialog) dialog.close(); });
  return { open, fresh };
}
