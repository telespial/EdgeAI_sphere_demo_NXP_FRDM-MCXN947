#!/usr/bin/env bash
set -euo pipefail

# Environment helper for MCUX builds. Keeps things user-local friendly.

export PATH="$HOME/.local/bin:$PATH"

if [[ -z "${ARMGCC_DIR:-}" ]]; then
  if ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    XP_BIN="$(ls -1d "$HOME/.local/opt"/xpack-arm-none-eabi-gcc-*/bin 2>/dev/null | sort | tail -n 1 || true)"
    if [[ -n "$XP_BIN" ]]; then
      export PATH="$XP_BIN:$PATH"
    fi
  fi

  if command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    ARMGCC_BIN="$(dirname "$(command -v arm-none-eabi-gcc)")"
    export ARMGCC_DIR="$(cd "$ARMGCC_BIN/.." && pwd)"
  else
    export ARMGCC_DIR="/usr"
  fi
fi

