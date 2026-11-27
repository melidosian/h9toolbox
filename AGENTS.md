# Agent guide: h9toolbox

Browser tools (`dist/`, deployed from `main` to h9toolbox.com) and firmware
research (`firmware-research/`) for the TIDRADIO TD-H9.

**Read first: [firmware-research/TD-H9-HANDBOOK.md](firmware-research/TD-H9-HANDBOOK.md)**,
which covers the hardware, memory maps, stock routines, recovery modes, rules
and dead ends. The full history is in
`firmware-research/resource-flash-log.md`; add every new finding there.

New machine: [firmware-research/setup/README.md](firmware-research/setup/README.md)
fetches and checks everything the builds and radio tools need that is not in
the repo (stock firmware, DMC100-Doom, Python packages, Docker images).

Short version of the rules (details in the handbook, section 1):

- Do not push, merge, or post publicly (comments, issues, PRs) unless the
  user asks.
- Never commit RAM dumps, full-chip dumps or radio backups: they hold
  personal settings.
- Writes to the radio's 16 MB chip go only into the erased tails, through a
  directory, so they can be restored.
- Every custom firmware keeps a way back to USB update mode.

## Firmware releases

- Treat `dev` as the staging branch on the path to `main`. In user-facing
  copy, call the tool the H9 Toolbox installer. Describe experimental
  features by their name and firmware version. Use `dev` only for branch
  and deployment details in engineering documentation.

- Before calling a firmware PR to `main` ready to merge, state the intended
  public release version and verify the default installer and App Manager
  catalog from the PR's production deployment output. Use the normal URLs
  without query parameters. Confirm the installer selects the intended
  firmware payloads, the payloads are published by the production workflow,
  and the catalog matches that firmware. A passing `dev` deployment only
  verifies the dev build.
- If production still defaults to an older release, report the exact version
  and treat the promotion as incomplete. Use the intended public version in
  the updater, firmware page, README, and PR description. Keep older builds
  available for upgrade or recovery when needed, without making them the
  default for the new release.

Tests: `node --test test/*.test.mjs` (site, uploader and
installer), plus the Python tests next
to each research tool.
