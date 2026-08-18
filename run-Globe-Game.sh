#!/usr/bin/env bash
# Open "Globe-Game" with its own dedicated sessions in a NEW TERMINAL WINDOW.
# Directory-agnostic: paths resolved from this file's location.

set -euo pipefail

# Locate and source box-env.sh (walks up to find the OpenCodeBox folder).
_d="$(cd "$(dirname "$0")" && pwd)"
[ -f "/run/media/hubertg/SONIC/OpenCodeBox/AGENTS.md" ] || BOX="/run/media/hubertg/SONIC"
while [ "$_d" != "/" ]; do
  if [ -f "$_d/scripts/box-env.sh" ] && [ -f "$_d/AGENTS.md" ]; then source "$_d/scripts/box-env.sh"; break; fi
  if [ -f "$_d/OpenCodeBox/scripts/box-env.sh" ]; then source "$_d/OpenCodeBox/scripts/box-env.sh"; break; fi
  _d="$(dirname "$_d")"
done

PROJ="Globe-Game"
PROJ_DATA="$BOX_DIR/data/projects-data"
PROJECTS_DIR="$BOX_DIR/projects"
DATA="$PROJ_DATA/Globe-Game/.opencode-data"
SESSIONS_DIR="$PROJ_DATA/Globe-Game/sessions"
HANDOFFS_DIR="$PROJ_DATA/Globe-Game/handoffs"
LOCKFILE="/tmp/opencode-Globe-Game.lock"

export OPENCODE_CONFIG="$PROJ_DATA/Globe-Game/opencode.json"

if [ "${1:-launch}" = "launch" ]; then
  # Single-instance guard: refuse a second terminal for this project unless
  # this is the auto-handoff continuation ("continue"). Only one agent/project.
  if [ -f "$LOCKFILE" ]; then
    LOCK_PID=
    if [ -n "$LOCK_PID" ] && kill -0 "$LOCK_PID" 2>/dev/null && [ "${2:-}" != "continue" ]; then
      echo "Globe-Game is already running (PID $LOCK_PID)."
      echo "Only one agent per project -- use the existing terminal, or close it first."
      exit 0
    fi
  fi
  if [ -z "${SPAWNED_TERMINAL:-}" ]; then detect_terminal || true; fi
  if [ -n "${SPAWNED_TERMINAL:-}" ]; then
    spawn_terminal "$0" run "${2:-}"
  else
    "$0" run "${2:-}" &
  fi
  # Auto-open the opencode sidebar once opencode is up and the window focused.
  scroll_target="$BOX_DIR/tools/openbox-keys"
  [ -x "$scroll_target" ] && sleep 1 && "$scroll_target" --delay 2.5 --force     --pidfile "$DATA/opencode.pid" --retries 10 >/dev/null 2>&1 &
  exit 0
fi

# "run" mode: inside the terminal window
cd "$PROJECTS_DIR/$PROJ"
mkdir -p "$DATA" "$SESSIONS_DIR" "$HANDOFFS_DIR"
if [ ! -e "$DATA/opencode/auth.json" ]; then
  mkdir -p "$DATA/opencode"
  ln -sf "$HOME/.local/share/opencode/auth.json" "$DATA/opencode/auth.json"
fi
export XDG_DATA_HOME="$DATA"
export OPENCODE_DISABLE_AUTOCOMPACT=1

# Token-price note at the start of every chat (peak vs off-peak DeepSeek hours).
if [ -f "$BOX_DIR/scripts/tokenprice.sh" ]; then
  bash "$BOX_DIR/scripts/tokenprice.sh" --notify 2>/dev/null || true
fi

# Acquire this project's single-instance lock (replacing the previous holder,
# which only happens legitimately on the auto-handoff continuation restart).
echo $$ > "$LOCKFILE"

# Remove the lock only if we still own it. The auto-handoff continuation can
# start a fresh terminal whose run mode re-claims this file before this old
# process exits; deleting it then would un-lock the new instance.
release_lock() {
  [ "$(cat "$LOCKFILE" 2>/dev/null || true)" = "$$" ] && rm -f "$LOCKFILE"
}

# On exit, export this project's sessions into its visible sessions/ folder.
cleanup() {
  release_lock
  mapfile -t IDs < <(opencode2 api --standalone get /api/session 2>/dev/null |     jq -r '.data[].id // empty' 2>/dev/null | grep -E '^ses_[A-Za-z0-9]+')
  if [ "${#IDs[@]}" -gt 0 ]; then
    rm -f "$SESSIONS_DIR"/*.json
    for id in "${IDs[@]}"; do
      opencode2 export --standalone -s "$id" > "$SESSIONS_DIR/$id.json" 2>/dev/null || true
    done
  fi
}
trap cleanup EXIT

PROMPT_ARGS=()
case "${2:-}" in
  continue) PROMPT_ARGS=(--prompt "Continue the work. Read ${HANDOFFS_DIR}/LATEST.md and follow its 'Next steps' in order. Do not re-ask settled questions.") ;;
  new) ;;
  *) PROMPT_ARGS=(--continue) ;;
esac

# Keep opencode attached to the real terminal (avoid dumping the TUI as text
# when backgrounded without a controlling stdin). --standalone gives this
# project its own private server + session DB under its XDG_DATA_HOME.
if [ -r /dev/tty ]; then
  opencode2 --standalone --auto "${PROMPT_ARGS[@]}" </dev/tty &
else
  opencode2 --standalone --auto "${PROMPT_ARGS[@]}" &
fi
echo $! > "$DATA/opencode.pid"

nohup bash "$BOX_DIR/scripts/auto-handoff.sh" --watch --name "Globe-Game" --data-dir "$DATA" --handoffs "$HANDOFFS_DIR" --pidfile "$DATA/opencode.pid" --workdir "$PROJECTS_DIR/$PROJ" --restart-cmd "$0 launch continue" >/dev/null 2>&1 &

# Nonstop addon: auto-resume after an LLM timeout (sibling project, if present).
if [ -f "$PROJECTS_DIR/nonstop/src/nonstop-watch.sh" ]; then
  nohup bash "$PROJECTS_DIR/nonstop/src/nonstop-watch.sh" \
    --log "$DATA/opencode/log/opencode.log" \
    --pidfile "$DATA/opencode.pid" \
    --typekeys "$PROJECTS_DIR/nonstop/src/typekeys.py" >/dev/null 2>&1 &
fi

# Autocommit addon: auto-commit stable changes (sibling project, if present).
if [ -f "$PROJECTS_DIR/autocommit/src/autocommit-watch.sh" ]; then
  nohup bash "$PROJECTS_DIR/autocommit/src/autocommit-watch.sh" \
    --workdir "$PROJECTS_DIR/$PROJ" \
    --pidfile "$DATA/opencode.pid" \
    --git "$BOX_DIR/plugins/git/git.sh">/dev/null 2>&1 &
fi

wait
