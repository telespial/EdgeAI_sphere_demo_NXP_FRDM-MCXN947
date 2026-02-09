# EdgeAI Sphere Demo Status

- Current target: FRDM-MCXN947 tilt-controlled ball demo on PAR-LCD-S035 + Accel 4 Click (FXLS8974CF over I2C)
- Workspace: `mcuxsdk_ws/` or `mcuxsdk_ws_test/` (created by `./tools/setup_mcuxsdk_ws.sh`)

## Golden Revision
If anything regresses, return to:
- Current golden tag: `GOLDEN_2026-02-09_v23_ball_polish_a1_a4`
- Commit: `git rev-parse GOLDEN_2026-02-09_v23_ball_polish_a1_a4`
- Baseline golden (older): `milestone_raster_flicker_npu_v9` @ `5d569d4352fc723f6d6d567dcdd3c46f58025fd4`

## Last Run
- Date: 2026-02-09
- Result: build + flash ok; LCD shows dune background + shaded "silver ball" with trails; impacts cause visible kicks
- Binary: `mcuxsdk_ws/build_v22_a3/edgeai_sand_demo_cm33_core0.bin`
- Notes:
  - NPU stack integrated (TFLM + Neutron backend). NPU stepping is compile-time gated (see `EDGEAI_ENABLE_NPU_INFERENCE` in `src/npu_api.h`).
  - Accel debug prints available on VCOM (`/dev/ttyACM0`)
  - Workspace patch applied for upstream TFLM kissfft tool build warning (see `tools/patch_mcuxsdk.sh`)
  - If the ball is stuck, check UART for `EDGEAI: accel ok addr=0x18` (or `0x19`). If “not found” appears, the sensor is not being detected (reseat the clickboard and confirm I2C mode).

## Background
- Dune background is generated from `downloads/sanddune.jpg` into `src/dune_bg.h` (see `tools/gen_dune_bg.py`).
- Depending on restore point:
  - `milestone_dune_bg_fullscreen_v1`: background is drawn full-screen at boot.
  - `milestone_dune_bg_tile_reveal_v1`: screen boots black and the background “reveals” as tiles are updated while the ball moves.
