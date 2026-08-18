#!/usr/bin/env bash
set -euo pipefail
# git-commit.sh — commit + push one or more projects.
#   git-commit.sh <Project> "message"           one project
#   git-commit.sh <Project...> "message"        several projects
# Projects are looked up under git-projects/ then projects/; "OpenCodeBox"
# means the box repo itself.

BOX_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"

ARGS=("$@")
MSG="${ARGS[-1]}"
NAMES=("${ARGS[@]:0:${#ARGS[@]}-1}")
[ ${#NAMES[@]} -gt 0 ] || { echo "usage: git-commit.sh <Project...> \"message\"" >&2; exit 1; }

resolve() {
  case "$1" in
    OpenCodeBox) echo "$BOX_DIR" ;;
    *) [ -d "$BOX_DIR/git-projects/$1" ] && echo "$BOX_DIR/git-projects/$1" || echo "$BOX_DIR/projects/$1" ;;
  esac
}

for name in "${NAMES[@]}"; do
  dir="$(resolve "$name")"
  if [ ! -d "$dir/.git" ]; then
    echo "  (skip) $name : no git repo at $dir"
    continue
  fi
  dirty="$(git -C "$dir" status --porcelain)"
  if [ -z "$dirty" ]; then
    echo "  (clean) $name"
    continue
  fi
  git -C "$dir" add -A
  git -C "$dir" commit -q -m "$MSG" && echo "  (committed) $name"
  if git -C "$dir" remote get-url origin >/dev/null 2>&1; then
    git -C "$dir" push -q 2>/dev/null && echo "  (pushed) $name" || echo "  (push failed) $name"
  else
    echo "  (no remote) $name"
  fi
done
echo DONE
