#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WS_DIR="${WS_DIR:-$ROOT_DIR/mcuxsdk_ws}"

# shellcheck disable=SC1090
source "$ROOT_DIR/tools/mcux_env.sh"

if [[ ! -d "$WS_DIR/.west" ]]; then
  echo "MCUX workspace missing at: $WS_DIR" >&2
  echo "Run: ./tools/setup_mcuxsdk_ws.sh" >&2
  exit 1
fi

if ! command -v LinkServer >/dev/null 2>&1; then
  echo "LinkServer not found on PATH." >&2
  echo "Install NXP LinkServer (x86_64) and ensure it is on PATH." >&2
  exit 1
fi

(
  cd "$WS_DIR"
  west flash -r linkserver
)

