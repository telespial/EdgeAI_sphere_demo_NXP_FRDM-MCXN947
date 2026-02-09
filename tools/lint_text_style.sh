#!/usr/bin/env bash
set -euo pipefail

# Fails if repo text contains conversational / direct reader references.
#
# Scope: repo-local sources and docs. Excludes MCUX workspaces and .git.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

# Keep patterns conservative to avoid false positives (e.g. loop variable i).
PATTERN='\\b(you|your|yours|we|our|ours|I|me|my|mine|ChatGPT|Codex|assistant|AI model|language model)\\b'

echo "[lint] Checking for conversational phrasing / direct reader references..."

if rg -n --hidden --follow -S "$PATTERN" \
  -g'!.git/**' \
  -g'!mcuxsdk_ws/**' \
  -g'!mcuxsdk_ws_test/**' \
  -g'*.md' \
  -g'*.sh' \
  -g'*.py' \
  -g'*.c' \
  -g'*.h' \
  -g'*.cpp' \
  -g'*.hpp' \
  docs README.md STATUS.md src sdk_example tools .; then
  echo >&2
  echo "[lint] FAIL: found banned phrasing. Rewrite to neutral, repo-centric wording." >&2
  echo "[lint] See: docs/STYLE_RULES.md" >&2
  exit 1
fi

echo "[lint] OK"

