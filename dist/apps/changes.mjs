// What "Write to radio" would change: modules added, removed, updated,
// renamed menu order. Modules are matched by name.
import { crc16 } from '../boot-logo/resource.mjs';

const same = (a, b) => a.body.length === b.body.length && crc16(a.body) === crc16(b.body) && a.bss === b.bss && a.entry === b.entry;

export function describeChanges(installed, staged) {
  const names = list => list.map(m => m.name);
  const before = new Map(installed.map(m => [m.name, m]));
  const after = new Map(staged.map(m => [m.name, m]));
  const added = staged.filter((m, i) => i > 0 && !before.has(m.name)).map(m => m.name);
  const removed = installed.filter((m, i) => i > 0 && !after.has(m.name)).map(m => m.name);
  const updated = staged.filter(m => before.has(m.name) && !same(m, before.get(m.name))).map(m => m.name);
  const menuReplaced = Boolean(installed.length && staged.length && (installed[0].name !== staged[0].name));
  const kept = names(staged).filter(n => before.has(n));
  const reordered = kept.join('\n') !== names(installed).filter(n => after.has(n)).join('\n');
  const firstInstall = !installed.length && staged.length > 0;
  const lines = [
    ...(firstInstall ? [`install ${staged.length} module(s) including the menu`] : []),
    ...(!firstInstall && added.length ? [`add ${added.join(', ')}`] : []),
    ...(removed.length ? [`remove ${removed.join(', ')}`] : []),
    ...(updated.length ? [`update ${updated.join(', ')}`] : []),
    ...(menuReplaced ? ['replace the menu'] : []),
    ...(reordered && !firstInstall ? ['new menu order'] : []),
  ];
  return { added, removed, updated, reordered, menuReplaced, lines, any: lines.length > 0 };
}
