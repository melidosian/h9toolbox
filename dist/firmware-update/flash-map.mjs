import { APP_START, IMAGE_SIZE, FLASH_SIZE, SECTOR_SIZE } from './firmware.mjs';
import { FAR_START, FAR_END } from './far-region.mjs?v=20261126sectors';

const regions = [
  { name: 'Boot', start: 0, end: APP_START, protected: true },
  { name: 'Main firmware', start: APP_START, end: IMAGE_SIZE },
  { name: 'Settings', start: IMAGE_SIZE, end: FAR_START, protected: true },
  { name: 'Far firmware', start: FAR_START, end: FAR_END },
  { name: 'Reserved', start: FAR_END, end: FLASH_SIZE, protected: true },
];
const labels = {
  planned: 'To write',
  writing: 'Writing',
  verified: 'Written and verified',
  untouched: 'Left alone',
  protected: 'Protected',
  interrupted: 'Write interrupted',
};
const hex = address => '0x' + address.toString(16).toUpperCase().padStart(5, '0');

export function createFlashMap(panel, grid, summary) {
  const blocks = new Map();
  for (const region of regions) {
    const row = document.createElement('div');
    row.className = 'flash-region';
    const label = document.createElement('strong');
    label.textContent = region.name;
    const cells = document.createElement('div');
    cells.className = 'flash-cells';
    cells.setAttribute('aria-hidden', 'true');
    for (let address = region.start; address < region.end; address += SECTOR_SIZE) {
      const cell = document.createElement('span');
      cell.className = 'flash-block';
      cells.append(cell);
      blocks.set(address, { cell, protected: region.protected, region: region.name });
    }
    row.append(label, cells);
    grid.append(row);
  }
  function paint(address, block, state) {
    block.cell.dataset.state = state;
    block.cell.title = `${hex(address)} · ${block.region} · ${labels[state]}`;
  }
  function report() {
    const counts = {};
    for (const {cell} of blocks.values()) {
      const state = cell.dataset.state;
      counts[state] = (counts[state] ?? 0) + 1;
    }
    summary.textContent = Object.entries(labels)
      .filter(([state]) => counts[state])
      .map(([state, label]) => `${counts[state]} ${label.toLowerCase()}`).join(' · ');
  }
  return {
    clear() { panel.hidden = true; },
    show(addresses) {
      const planned = new Set(addresses);
      for (const [address, block] of blocks) {
        paint(address, block, block.protected ? 'protected' : planned.has(address) ? 'planned' : 'untouched');
      }
      panel.hidden = false;
      report();
    },
    mark(address, state) {
      const block = blocks.get(address);
      if (block && !block.protected) {
        paint(address, block, state);
        report();
      }
    },
    interrupt() {
      for (const [address, block] of blocks) {
        if (block.cell.dataset.state === 'writing') paint(address, block, 'interrupted');
      }
      if (!panel.hidden) report();
    },
  };
}
