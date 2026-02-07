#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WS_DIR="${WS_DIR:-$ROOT_DIR/mcuxsdk_ws}"
BUILD_TYPE="${1:-debug}"

# shellcheck disable=SC1090
source "$ROOT_DIR/tools/mcux_env.sh"

if [[ ! -d "$WS_DIR/.west" ]]; then
  echo "MCUX workspace missing at: $WS_DIR" >&2
  echo "Run: ./tools/setup_mcuxsdk_ws.sh" >&2
  exit 1
fi

(
  cd "$WS_DIR"
  west build -p always mcuxsdk/examples/demo_apps/edgeai_sand_demo \
    --toolchain armgcc \
    --config "$BUILD_TYPE" \
    -b frdmmcxn947 \
    -Dcore_id=cm33_core0
)

echo "Built: $WS_DIR/build/edgeai_sand_demo_cm33_core0.bin"

