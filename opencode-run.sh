#!/usr/bin/env bash
set -euo pipefail

# Open "Globe-Game" with its own dedicated sessions.
PROJ="Globe-Game"
DRIVE="/run/media/hubertg/SONIC"
DATA="/run/media/hubertg/SONIC/OpenCodeBox/project-data/Globe-Game/.opencode-data"
SESSIONS_DIR="/run/media/hubertg/SONIC/OpenCodeBox/project-data/Globe-Game/sessions"

cd "$DRIVE/$PROJ"
mkdir -p "$DATA" "$SESSIONS_DIR"

if [ ! -e "$DATA/opencode/auth.json" ]; then
  mkdir -p "$DATA/opencode"
  ln -sf "$HOME/.local/share/opencode/auth.json" "$DATA/opencode/auth.json"
fi

export XDG_DATA_HOME="$DATA"
export OPENCODE_CONFIG="/run/media/hubertg/SONIC/OpenCodeBox/project-data/Globe-Game/opencode.json"

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
