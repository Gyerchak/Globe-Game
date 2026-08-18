# Environment knowledge (core)

## Where you run

- **Machine:** a Linux PC (Arch/CachyOS-based desktop with KDE, zsh default shell).
- **You are opencode2 (V2 beta)** — installed at `~/.opencode/bin/opencode2` —
  running inside **OpenCodeBox**, a custom closed-environment setup built around
  opencode2. You are NOT plain opencode: your behaviour, buttons, limits and
  workflows are controlled by this box (see the sections below).
- The beta changes quickly; if a tool/flag behaves unexpectedly, verify against
  `opencode2 --help` and the official docs (https://opencode.ai/v2/docs/) before
  assuming it is broken.

## The OpenCodeBox structure

- The box lives in one folder (currently `/run/media/hubertg/SONIC/OpenCodeBox/`)
  and is **directory-agnostic**: if the whole folder moves, everything adapts.
- `box/` is the shared brain:
  - `box/agents/` — the layered agent settings (modes → logics → knowledges →
    instructions → characters → commands, plus deepthinking, reasoning,
    contextlimit, thinklimit, memory, classic). The root `AGENTS.md` you are
    reading is COMPOSED from these layers by `box/scripts/tools/compose-agents.sh`.
  - `box/skills/` — official opencode2 skills (ponytail, grilling, wayfinder, ask-matt).
  - `box/scripts/` — `tools/` (regularly used: launcher, toggles, limits,
    handoff, config generator…), `helpers/` (occasional: memory, git, token
    price…), `dump/` (one-time helper files made by the box; do not treat as
    stable tooling).
  - `box/plugins/` + per-plugin `addons/` — extension structure reserved for
    future growth.
  - `box/sessions/` — this terminal's own opencode data (sessions DB, logs).
  - `box/data/` — runtime state: handoffs, notes, limit logs.
  - `box/backup/` — backups (srcbackups/latest + archive).
  - `box/waste/` — the local trash bin. **The ONLY allowed way to delete
    anything is moving it into waste/** (`mv target box/waste/`). Never `rm`
    user content — if you truly must remove a file, move it to waste.
  - `box/preferences.cfg` — ordered preferences per category (code, graphics,
    ram…). Follow them in order; only if a preference fails may you use another
    way, and then say so.
  - `box/TokenKeysMCP.env` — SECRETS (gitignored). Never print or commit it.
  - `box/cli.json` — TUI settings and keybinds.
- `git-projects/` — projects that live on git (GitHub). Each has its own repo,
  its own `box/` copy of the brain, its own `<Project>.desktop` and launcher in
  its own `box/scripts/`.
- `projects/` — LOCAL-ONLY projects (never published to git), same structure.
- `src/` (box root) and `<project>/src/` — code written for the box. **Prefer
  C++20 / C16** (see `box/preferences.cfg`).
- `docs/` — SETUP.md, GITHUB.md, STRUCTURE.md; `README.detailed.md` is the full guide.

## A project looks like

```
<Project>/
  <Project>.desktop          # its own terminal launcher
  AGENTS.md                  # its own composed agent file
  opencode.json              # its own config (generated, permissions scoped)
  box/                       # its own box copy (agents, skills, scripts, …)
  content/ data/ docs/ exe/ files/ input/ logs/ output/ src/ tmp/ tools/ web/
  README.md  LICENSE  .gitignore
```

## Edit boundary (strictly enforced by opencode permissions)

- **Read:** anywhere on the computer.
- **Write:** ONLY inside the box container (the drive folder holding
  OpenCodeBox, including all projects inside it) **and** `/tmp/opencodebox/`.
- `/tmp/opencodebox/` is the only writable place outside the box — it lives in
  RAM. Create it if missing (`mkdir -p /tmp/opencodebox`). It may optionally be
  space-limited (see `box/preferences.cfg` `[ram]`; default limit OFF, 6 GiB
  when enabled). Use it for scratch files, big temporary outputs, builds in
  tmpfs — not for anything that must survive a reboot.
- Anything outside the boundary requires a write → stop and tell the user.
  Never try to bypass the boundary (no sudo tricks, no symlink escapes).

## Shell

- Shell/bash is fully allowed inside the boundary (no tee workarounds needed).
- Prefer small, idempotent commands; show output; explain destructive steps.

## Context sources available to you (in priority order)

1. **Memory** — if memory usage is ON (`/memoryuse`), consult the memory store
   first (project `box/data/notes` + memory plugin files).
2. **Session history** — previous sessions of this terminal (the TUI session
   list, and `box/data/…` exports).
3. **Backup files** — `box/backup/srcbackups/latest/<Project>/` (and `archive/`).
4. **GitHub history** — the project's git log / remote repo state.

## Thinking language

Think in whatever language/notation is most efficient for you — if the source
material is in Chinese, think in Chinese; if it is math, think in math; if it
is code, think in code. The user only needs to understand your ANSWERS:
always reply in the language the user is writing in.

## Secrets

Never print, log, or commit API keys/tokens. `TokenKeysMCP.env` files are
gitignored — if you see one staged in git, stop and warn the user.
