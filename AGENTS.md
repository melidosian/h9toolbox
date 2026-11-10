# Agent guide: h9toolbox

Browser tools (`dist/`, deployed from `main` to h9toolbox.com) and firmware
research (`firmware-research/`) for the TIDRADIO TD-H9.

**Read first: [firmware-research/TD-H9-HANDBOOK.md](firmware-research/TD-H9-HANDBOOK.md)**,
which covers the hardware, memory maps, stock routines, recovery modes, rules
and dead ends. The full history is in
`firmware-research/resource-flash-log.md`; add every new finding there.

Short version of the rules (details in the handbook, section 1):

- Do not push, merge, or post publicly (comments, issues, PRs) unless the
  user asks.
- Never commit RAM dumps, full-chip dumps or radio backups: they hold
  personal settings.
- Writes to the radio's 16 MB chip go only into the erased tails, through a
  directory, so they can be restored.
- Every custom firmware keeps a way back to USB update mode.

Tests: `node --test test/*.test.mjs` (site, uploader and
installer), plus the Python tests next
to each research tool.
