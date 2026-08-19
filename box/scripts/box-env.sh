#!/usr/bin/env bash
# box-env.sh — shared helpers for OpenCodeBox scripts.
# Source this file (never execute). Resolves all paths dynamically from its
# own location, so the whole setup is directory-agnostic.
#
#   BOX_DIR        = the box root (OpenCodeBox/ or a project dir that has box/)
#   AGENTS_DIR     = BOX_DIR/box/agents
#   SCRIPTS_DIR    = BOX_DIR/box/scripts
#   SESSIONS_DIR   = BOX_DIR/box/sessions        (opencode's own data home)
#   CFG_DIR        = BOX_DIR/box/cfg             (isolated XDG config home)
#   (runtime state lives under SESSIONS_DIR: notes, limitlogs, memory, exports)
#   RAM_DIR        = /tmp/opencodebox            (RAM scratch, only external write dir)

if [ -z "${BOX_DIR:-}" ]; then
  BOX_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
fi
AGENTS_DIR="$BOX_DIR/box/agents"
SCRIPTS_DIR="$BOX_DIR/box/scripts"
SESSIONS_DIR="$BOX_DIR/box/sessions"
CFG_DIR="$BOX_DIR/box/cfg"
BACKUP_DIR="$BOX_DIR/box/backup"
WASTE_DIR="$BOX_DIR/box/waste"
RAM_DIR="/tmp/opencodebox"
# runtime state lives under sessions/ (no legacy box/data/ dir anymore)
NOTES_DIR="$SESSIONS_DIR/notes"
LIMIT_DIR="$SESSIONS_DIR/limitlogs"
MEM_DIR="$SESSIONS_DIR/memory"
EXPORTS_DIR="$SESSIONS_DIR/exports"
SETUPS_DIR="$AGENTS_DIR/setups"

# opencode2 ships as a standalone binary in ~/.opencode/bin; spawned terminals
# are non-login shells, so make sure it is always resolvable.
case ":$PATH:" in
  *":$HOME/.opencode/bin:"*) ;;
  *) export PATH="$HOME/.opencode/bin:$PATH" ;;
esac

# ── Terminal spawner ───────────────────────────────────────────────────────
SPAWNED_TERMINAL=""
detect_terminal() {
  local candidates=(konsole gnome-terminal alacritty terminator kitty xfce4-terminal foot wezterm tilix xterm lxterminal sakura)
  for t in "${candidates[@]}"; do
    if command -v "$t" >/dev/null 2>&1; then SPAWNED_TERMINAL="$t"; return 0; fi
  done
  return 1
}
spawn_terminal() {
  if ! detect_terminal; then
    echo "No GUI terminal emulator found; running in current shell." >&2
    bash -c "$*"
    return $?
  fi
  local cmd
  cmd="$(printf '%q ' "$@"); $SHELL"
  case "$SPAWNED_TERMINAL" in
    konsole)        konsole --nofork -e bash -c "$cmd" >/dev/null 2>&1 & ;;
    gnome-terminal) gnome-terminal -- bash -c "$cmd" >/dev/null 2>&1 & ;;
    alacritty)      alacritty -e bash -c "$cmd" >/dev/null 2>&1 & ;;
    terminator)     terminator -e "bash -c \"$cmd\"" >/dev/null 2>&1 & ;;
    kitty)          kitty bash -c "$cmd" >/dev/null 2>&1 & ;;
    xfce4-terminal) xfce4-terminal -e "bash -c \"$cmd\"" >/dev/null 2>&1 & ;;
    foot)           foot bash -c "$cmd" >/dev/null 2>&1 & ;;
    wezterm)        wezterm start -- bash -c "$cmd" >/dev/null 2>&1 & ;;
    tilix)          tilix -e bash -c "$cmd" >/dev/null 2>&1 & ;;
    xterm)          xterm -e bash -c "$cmd" >/dev/null 2>&1 & ;;
    lxterminal)     lxterminal -e "bash -c \"$cmd\"" >/dev/null 2>&1 & ;;
    sakura)         sakura -e bash -c "$cmd" >/dev/null 2>&1 & ;;
    *)              echo "Unsupported terminal: $SPAWNED_TERMINAL" >&2; return 1 ;;
  esac
  disown 2>/dev/null || true
  return 0
}
