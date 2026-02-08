# Restore Points

This file is an **append-only index** of known-good restore points.

Source of truth:
- Use the git tags listed below to restore code quickly.
- Do not delete old entries. Prefer adding a new tag + a new entry.

## How To Restore
```bash
cd /path/to/EdgeAI_sand_demo
git fetch --tags
git checkout <TAG>
MCUX_EXAMPLES_DIR="$PWD/mcuxsdk_ws_test/mcuxsdk/examples" ./sdk_example/install_mcux_overlay.sh
ninja -C mcuxsdk_ws_test/build
WS_DIR="$PWD/mcuxsdk_ws_test" ./tools/flash_frdmmcxn947.sh
```

## Restore Points

### 2026-02-08 Golden (Raster Ball + Trails)
- Tag: `GOLDEN_2026-02-08_v9`
- Also: `milestone_raster_flicker_npu_v9`
- Commit: `5d569d4352fc723f6d6d567dcdd3c46f58025fd4`
- Hardware: FRDM-MCXN947 + PAR-LCD-S035 + Accel 4 Click (FXLS8974CF over mikroBUS/I2C)
- Behavior: tilt-controlled silver ball with shadow + trails (raster renderer)
- Notes: keep this as the baseline restore point if anything breaks.

## Template (Copy/Paste)
### YYYY-MM-DD Short Name
- Tag: `TAG_NAME`
- Commit: `FULL_SHA1`
- Hardware:
- Behavior:
- Notes:

