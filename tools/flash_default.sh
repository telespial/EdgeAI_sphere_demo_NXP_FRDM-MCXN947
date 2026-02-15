#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FAILSAFE_DOC="$ROOT_DIR/docs/failsafe.md"

if [[ ! -f "$FAILSAFE_DOC" ]]; then
  echo "FLASH_DEFAULT: missing pointer file: $FAILSAFE_DOC" >&2
  exit 1
fi

expected="$(sed -n '1p' "$FAILSAFE_DOC" | tr -d '\r')"
if [[ -z "$expected" ]]; then
  echo "FLASH_DEFAULT: empty failsafe filename in: $FAILSAFE_DOC" >&2
  exit 1
fi

echo "FLASH_DEFAULT: flashing pinned failsafe artifact: $expected"
FAILSAFE_CONFIRM="$expected" "$ROOT_DIR/tools/flash_failsafe.sh"
