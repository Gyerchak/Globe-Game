#!/usr/bin/env bash
# ghh.sh -- gh passthrough helper for the box (gh subcommands beyond the
# opencode.json whitelist run through here; use: bash script/ghh.sh <args...>)
set -euo pipefail
exec gh "$@"
