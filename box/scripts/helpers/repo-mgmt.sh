#!/usr/bin/env bash
# repo-mgmt.sh — scaffold + create GitHub repos for project folders.
# Usage:
#   repo-mgmt.sh <Project> <public|private|list|repos>
#     <Project> public|private   init git + scaffold + create GitHub repo
#     list                       list project folders NOT yet git repos
#     repos                      list all project folders (for /repos)

set -euo pipefail

BOX_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"

DEFAULT_DIRS="src data input output exe tmp web content logs scripts docs plugins tools"

project_folders() {
  local d base
  for base in "$BOX_DIR/git-projects" "$BOX_DIR/projects"; do
    [ -d "$base" ] || continue
    for d in "$base"/*/; do
      [ -d "$d" ] || continue
      echo "$base/$(basename "$d")"
    done
  done
}

owner() {
  local l
  l="$(gh api user --jq .login 2>/dev/null || true)"
  [ -n "$l" ] || l="$(git config --global user.name || true)"
  [ -n "$l" ] || l="$(whoami)"
  echo "$l"
}

scaffold() {
  local dir="$1" proj own year
  proj="$(basename "$dir")"
  own="$(owner)"
  year="$(date +%Y)"

  local d
  for d in $DEFAULT_DIRS; do
    [ -d "$dir/$d" ] || mkdir -p "$dir/$d"
  done
  mkdir -p "$dir/plugins/addons" "$dir/tools"

  if [ ! -f "$dir/.gitignore" ]; then
    printf 'tools/\nplugins/\ndata/\nbox/TokenKeysMCP.env\nbox/sessions/\nbox/backup/\nbox/waste/\n'>"$dir/.gitignore"
  fi

  if [ ! -f "$dir/LICENSE" ]; then
    printf 'MIT License\n\nCopyright (c) %s %s\n\nPermission is hereby granted, free of charge, to any person obtaining a copy\nof this software and associated documentation files (the "Software"), to deal\nin the Software without restriction, including without limitation the rights\nto use, copy, modify, merge, publish, distribute, sublicense, and/or sell\ncopies of the Software, and to permit persons to whom the Software is\nfurnished to do so, subject to the following conditions:\n\nThe above copyright notice and this permission notice shall be included in all\ncopies or substantial portions of the Software.\n\nTHE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\nIMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\nFITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\nAUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\nLIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\nOUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\nSOFTWARE.\n' "$year" "$own">"$dir/LICENSE"
  fi

  if [ ! -f "$dir/README.md" ]; then
    printf '# %s\n\nProject scaffolded by OpenCodeBox on %s for %s.\n' "$proj" "$(date '+%Y-%m-%d')" "$own">"$dir/README.md"
  fi
}

init_one() {
  local dir="$1" vis="$2"
  local proj; proj="$(basename "$dir")"
  [ -d "$dir" ] || { echo "  (error) $proj : folder not found"; return; }
  if [ -d "$dir/.git" ]; then
    echo "  (exists) $proj : already a git repo"
    return
  fi
  scaffold "$dir"
  git -C "$dir" init -q
  git -C "$dir" branch -m main
  # copy-paste the main box template into the new project + generate its files
  bash "$BOX_DIR/box/scripts/tools/make-project-runs.sh" "$proj" >/dev/null 2>&1 || true
  git -C "$dir" add -A
  git -C "$dir" commit -q -m "chore: scaffold $proj"
  gh repo create "$proj" --$vis --source "$dir" --push || true
  echo "  -> $proj : $vis"
}

case "${2:-${1:-list}}" in
  list)
    echo "Project folders that are NOT git repos yet:"
    for p in $(project_folders); do
      [ -d "$p/.git" ] || echo "  - $p"
    done
    ;;
  repos)
    echo "Project folders:"
    for p in $(project_folders); do
      st="no-git"
      [ -d "$p/.git" ] && st="git"
      echo "  - $p ($st)"
    done
    ;;
  public|private)
    init_one "$1" "$2"
    ;;
  *)
    echo "usage: repo-mgmt.sh <Project> <public|private|list|repos>"
    exit 1
    ;;
esac
