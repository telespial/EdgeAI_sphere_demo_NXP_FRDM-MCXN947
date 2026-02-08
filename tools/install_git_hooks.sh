#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
hooks_src="$repo_root/tools/git-hooks"
hooks_dst="$repo_root/.git/hooks"

if [[ ! -d "$hooks_dst" ]]; then
  echo "ERROR: .git/hooks not found at: $hooks_dst" >&2
  echo "Run this from inside a git checkout." >&2
  exit 1
fi

install_one() {
  local name="$1"
  local src="$hooks_src/$name"
  local dst="$hooks_dst/$name"

  if [[ ! -f "$src" ]]; then
    echo "ERROR: missing hook template: $src" >&2
    exit 1
  fi

  install -m 0755 "$src" "$dst"
  echo "Installed: .git/hooks/$name"
}

install_one pre-push

