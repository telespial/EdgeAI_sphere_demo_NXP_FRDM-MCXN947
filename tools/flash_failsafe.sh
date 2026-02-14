#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FAILSAFE_DOC="$ROOT_DIR/docs/failsafe.md"

if [[ ! -f "$FAILSAFE_DOC" ]]; then
  echo "FAILSAFE: missing pointer file: $FAILSAFE_DOC" >&2
  exit 1
fi

expected="$(sed -n '1p' "$FAILSAFE_DOC" | tr -d '\r')"
desc="$(sed -n '2p' "$FAILSAFE_DOC" | tr -d '\r')"

if [[ -z "$expected" ]]; then
  echo "FAILSAFE: empty failsafe filename in: $FAILSAFE_DOC" >&2
  exit 1
fi

failsafe_path="$ROOT_DIR/$expected"
if [[ ! -f "$failsafe_path" ]]; then
  echo "FAILSAFE: failsafe artifact not found: $failsafe_path" >&2
  exit 1
fi

confirm="${FAILSAFE_CONFIRM:-}"
if [[ $# -ge 1 ]]; then
  confirm="$1"
fi

if [[ "$confirm" != "$expected" ]]; then
  echo "FAILSAFE: confirmation required" >&2
  echo "FAILSAFE: expected filename: $expected" >&2
  echo "FAILSAFE: description: $desc" >&2
  echo "FAILSAFE: pass argv1 as the expected filename or set FAILSAFE_CONFIRM" >&2
  exit 2
fi

if ! command -v LinkServer >/dev/null 2>&1; then
  echo "FAILSAFE: LinkServer not found on PATH" >&2
  exit 1
fi

device="${FAILSAFE_DEVICE:-auto}"

probe_args=()
if [[ -n "${PROBE:-}" ]]; then
  probe_args+=(--probe "$PROBE")
fi

# Do not use --erase-all with this ELF: LinkServer may mass-erase between
# segments and wipe the vector table at 0x00000000 on MCXN947.
LinkServer flash "${probe_args[@]}" "$device" load "$failsafe_path"
