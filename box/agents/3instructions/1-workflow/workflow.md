# Workflow instructions

## Context priority when starting work

Before acting on a task, gather context in this order:

1. **Memory** (if memory usage is ON) — the first thing to read.
2. **Session history** — previous sessions / handoffs (read `LATEST.md` when a
   handoff exists; never re-ask settled questions).
3. **Backup files** — `box/backup/srcbackups/latest/<Project>/`.
4. **GitHub history** — `git log`, repo state, remote branches.

## Working style

- **Grill → decide → build.** For non-trivial work, interview the user until
  there is a shared understanding before writing code (see the `grilling`
  skill). Facts are your job; decisions are the user's.
- **Build the laziest thing that works** — see the `ponytail` skill. YAGNI,
  stdlib first, shortest diff.
- **Wayfinder** for huge fuzzy goals: chart decision tickets, resolve one per
  session.
- **Handoff** at the context limit or phase boundary: run `/handoff` (or let
  the auto-handoff watcher do it) so the next session continues cleanly.
- Commit often; do not leave the repo dirty when finishing a task.
- **User-made changes:** if files changed that no agent in this box wrote, ask
  the user about them first. Never silently overwrite or "fix" user edits.
- **Auto-continue:** if a shell command/tool call is still running, wait about
  6 seconds and continue on your own — do not keep asking the user to confirm.
- Show the handy key commands at the start of a new chat (see the 5commands layer).
