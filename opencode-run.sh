#!/usr/bin/env bash
# Open "Globe-Game" with its own dedicated sessions in a NEW TERMINAL WINDOW.
# Directory-agnostic: paths resolved from this file's location.

set -euo pipefail

# Locate and source box-env.sh (walks up to find the OpenCodeBox folder).
_d="$(cd "$(dirname "$0")" && pwd)"
while [ "$_d" != "/" ]; do
  if [ -f "$_d/box-env.sh" ]; then source "$_d/box-env.sh"; break; fi
  if [ -f "$_d/OpenCodeBox/box-env.sh" ]; then source "$_d/OpenCodeBox/box-env.sh"; break; fi
  _d="$(dirname "$_d")"
done

PROJ="Globe-Game"
PROJ_DATA="$BOX_DIR/project-data"
DATA="$PROJ_DATA/Globe-Game/.opencode-data"
SESSIONS_DIR="$PROJ_DATA/Globe-Game/sessions"
HANDOFFS_DIR="$PROJ_DATA/Globe-Game/handoffs"

export OPENCODE_CONFIG="$PROJ_DATA/Globe-Game/opencode.json"

if [ "${1:-launch}" = "launch" ]; then
  if [ -z "${SPAWNED_TERMINAL:-}" ]; then detect_terminal || true; fi
  if [ -n "${SPAWNED_TERMINAL:-}" ]; then
    spawn_terminal "$0" run "${2:-}"
  else
    "$0" run "${2:-}" &
  fi
  # Auto-open the opencode sidebar in the new window shortly after it starts.
  scroll_target="$BOX_DIR/openbox-keys.py"
  [ -f "$scroll_target" ] && sleep 1 && python3 "$scroll_target" --delay 2.5 >/dev/null 2>&1 &
  exit 0
fi

# "run" mode: inside the terminal window
cd "$CONTAINER_DIR/$PROJ"
mkdir -p "$DATA" "$SESSIONS_DIR" "$HANDOFFS_DIR"
if [ ! -e "$DATA/opencode/auth.json" ]; then
  mkdir -p "$DATA/opencode"
  ln -sf "$HOME/.local/share/opencode/auth.json" "$DATA/opencode/auth.json"
fi
export XDG_DATA_HOME="$DATA"
export OPENCODE_DISABLE_AUTOCOMPACT=1

# On exit, export this project's sessions into its visible sessions/ folder.
cleanup() {
  mapfile -t IDs < <(opencode session list 2>/dev/null | grep -E '^ses_[A-Za-z0-9]+' | awk '{print $1}')
  if [ "${#IDs[@]}" -gt 0 ]; then
    rm -f "$SESSIONS_DIR"/*.json
    for id in "${IDs[@]}"; do
      opencode export "$id" > "$SESSIONS_DIR/$id.json" 2>/dev/null || true
    done
  fi
}
trap cleanup EXIT

PROMPT_ARGS=()
if [ "${2:-}" = "continue" ]; then
  PROMPT_ARGS=(--prompt "Continue the work. Read ${HANDOFFS_DIR}/LATEST.md and follow its 'Next steps' in order. Do not re-ask settled questions.")
fi

opencode --auto "${PROMPT_ARGS[@]}" &
echo $! > "$DATA/opencode.pid"

nohup bash "$BOX_DIR/auto-handoff.sh" --watch --name "Globe-Game" --data-dir "$DATA" --handoffs "$HANDOFFS_DIR" --pidfile "$DATA/opencode.pid" --workdir "$CONTAINER_DIR/$PROJ" --restart-cmd "$0 launch continue" >/dev/null 2>&1 &

wait
