#!/usr/bin/env bash
set -euo pipefail

# Patch known build-breakers in the upstream MCUX SDK checkout.
# This repo aims to be "clone + run scripts" reproducible without manual edits.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WS_DIR="${1:-${WS_DIR:-$ROOT_DIR/mcuxsdk_ws}}"

KISS_FASTFIR_C="$WS_DIR/mcuxsdk/middleware/eiq/tensorflow-lite/third_party/kissfft/tools/kiss_fastfir.c"
EDGEAI_CMAKELISTS="$WS_DIR/mcuxsdk/examples/demo_apps/edgeai_sand_demo/CMakeLists.txt"

echo "[patch] ws: $WS_DIR"

if [[ -f "$KISS_FASTFIR_C" ]]; then
  # GCC -Werror + -Wunused-variable can make this upstream "tools" source fail.
  # Make it deterministic and idempotent.
  if grep -q "__attribute__\\s*(([[:space:]]*unused[[:space:]]*))" "$KISS_FASTFIR_C"; then
    echo "[patch] ok: kiss_fastfir.c already patched"
  else
    if grep -qE '^static int verbose=0;[[:space:]]*$' "$KISS_FASTFIR_C"; then
      echo "[patch] fix: kiss_fastfir.c unused verbose"
      perl -0777 -pi -e 's|^static int verbose=0;\\s*$|/* Patched by EdgeAI_sand_demo: MCUX SDK builds with -Werror; mark unused. */\\nstatic int verbose __attribute__((unused)) = 0;|m' "$KISS_FASTFIR_C"
    else
      echo "[patch] warn: kiss_fastfir.c verbose line not found; leaving unmodified"
    fi
  fi
else
  echo "[patch] skip: not found: $KISS_FASTFIR_C"
fi

# Ensure the example wrapper pulls in repo sources (when the workspace was created
# before adding new files).
if [[ -f "$EDGEAI_CMAKELISTS" ]]; then
  if ! grep -q 'src/sw_render.c' "$EDGEAI_CMAKELISTS"; then
    echo "[patch] fix: add src/sw_render.c to edgeai_sand_demo CMakeLists"
    perl -0777 -pi -e 's/(SOURCES\\s+src\\/edgeai_sand_demo\\.c\\s+\\s*src\\/fxls8974cf\\.c\\s+\\s*src\\/par_lcd_s035\\.c\\s+)/$1            src\\/sw_render\\.c\\n            /m' "$EDGEAI_CMAKELISTS" || true
  fi
fi
