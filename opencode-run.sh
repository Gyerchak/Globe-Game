#!/usr/bin/env bash
# Open "Globe-Game" with its own dedicated sessions in a NEW TERMINAL WINDOW.
# Directory-agnostic: paths resolved from this file's location.

set -euo pipefail

# The box is the parent of the project-runs/ folder (or the folder holding this
# run file's box). Resolve the box directory robustly:
if [ -f "$(dirname "$0")/../box-env.sh" ]; then
  BOX="$(cd "$(dirname "$0")/.." && pwd)"
else
  BOX="$(cd "$(dirname "$0")" && pwd)"
fi
CONTAINER="$(dirname "$BOX")"
PROJ="Globe-Game"
PROJ_DATA="$BOX/project-data"
DATA="$PROJ_DATA/Globe-Game/.opencode-data"
SESSIONS_DIR="$PROJ_DATA/Globe-Game/sessions"

export OPENCODE_CONFIG="$PROJ_DATA/Globe-Game/opencode.json"

if [ "${1:-launch}" = "launch" ]; then
  source "$BOX/box-env.sh"
  if [ -z "${SPAWNED_TERMINAL:-}" ]; then detect_terminal || true; fi
  if [ -n "${SPAWNED_TERMINAL:-}" ]; then
    spawn_terminal "$0" run
  else
    exec "$0" run
  fi
  exit 0
fi

# "run" mode: inside the terminal window
cd "$CONTAINER/$PROJ"
mkdir -p "$DATA" "$SESSIONS_DIR"
if [ ! -e "$DATA/opencode/auth.json" ]; then
  mkdir -p "$DATA/opencode"
  ln -sf "$HOME/.local/share/opencode/auth.json" "$DATA/opencode/auth.json"
fi
export XDG_DATA_HOME="$DATA"

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

opencode --auto
