#!/usr/bin/env bash
set -u
# commit-all.sh — autocommit every repo in the box (projects + the box itself).
# Used by the autocommit helper; skips clean repos.

BOX_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"

commit_repo() {
  local repo="$1" name="$2"
  [ -d "$repo/.git" ] || return
  local dirty
  dirty="$(git -C "$repo" status --porcelain)"
  [ -n "$dirty" ] || { echo "(clean) $name"; return; }
  local count example
  count="$(git -C "$repo" status --porcelain | wc -l)"
  example="$(git -C "$repo" status --porcelain | head -1 | awk '{print $2}')"
  local msg="chore: autocommit $name: $count file(s), e.g. $example"
  git -C "$repo" add -A
  git -C "$repo" commit -q -m "$msg" && echo "(committed) $name"
}

for base in "$BOX_DIR/git-projects" "$BOX_DIR/projects"; do
  [ -d "$base" ] || continue
  for d in "$base"/*/; do
    [ -d "$d" ] || continue
    commit_repo "$d" "$(basename "$d")"
  done
done
commit_repo "$BOX_DIR" "OpenCodeBox"
echo DONE
