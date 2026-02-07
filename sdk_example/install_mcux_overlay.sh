#!/usr/bin/env bash
set -euo pipefail

# Copy the MCUX SDK examples overlay (this repo) into the checked-out MCUX examples tree.
#
# This keeps the build entrypoint inside mcuxsdk-examples while keeping editable code in this repo.

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OVERLAY_DIR="$HERE/mcuxsdk_examples_overlay"

CODEMASTER_ROOT="${CODEMASTER_ROOT:-$(cd "$HERE/../../../../.." && pwd)}"
MCUX_EXAMPLES_DIR="${MCUX_EXAMPLES_DIR:-$CODEMASTER_ROOT/vendors/nxp/mcuxpresso-sdk/mcuxsdk/examples}"

if [[ ! -d "$OVERLAY_DIR/demo_apps/edgeai_sand_demo" ]]; then
  echo "Overlay not found at: $OVERLAY_DIR" >&2
  exit 1
fi

if [[ ! -d "$MCUX_EXAMPLES_DIR" ]]; then
  echo "MCUX examples dir not found at: $MCUX_EXAMPLES_DIR" >&2
  echo "Set MCUX_EXAMPLES_DIR to your mcuxsdk-examples checkout." >&2
  exit 1
fi

mkdir -p "$MCUX_EXAMPLES_DIR/demo_apps" "$MCUX_EXAMPLES_DIR/_boards/frdmmcxn947/demo_apps"

cp -a "$OVERLAY_DIR/demo_apps/edgeai_sand_demo" "$MCUX_EXAMPLES_DIR/demo_apps/"
cp -a "$OVERLAY_DIR/_boards/frdmmcxn947/demo_apps/edgeai_sand_demo" "$MCUX_EXAMPLES_DIR/_boards/frdmmcxn947/demo_apps/"

echo "Installed edgeai_sand_demo wrapper into: $MCUX_EXAMPLES_DIR"

