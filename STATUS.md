# EdgeAI Sphere Demo Status

- Current target: FRDM-MCXN947 tilt-controlled ball demo on PAR-LCD-S035 + Accel 4 Click (FXLS8974CF over I2C)
- Workspace: `mcuxsdk_ws/` or `mcuxsdk_ws_test/` (created by `./tools/setup_mcuxsdk_ws.sh`)

## Functional Summary
- Controls:
  - Tilt (LP accel) drives X/Y acceleration.
  - Vertical motion (HP of accel magnitude `|a|`) drives a latched Z scale (ball grows/shrinks and holds last scale).
  - "Bang" impulse (HP accel score) provides a one-shot velocity kick.
- Rendering:
  - Dune background (generated from `downloads/sanddune.jpg` into `src/dune_bg.h`)
  - Motion trails + shadow + reflective silver ball shader (environment reflection + moving sparkles)
  - HUD: FPS and NPU state (`B:? N:? I:?`)
- NPU:
  - Backend options: stub (`B:S`) or Neutron (`B:N`).
  - When inference is enabled, NPU output is reduced to a single `glint` value (0..255) that modulates specular intensity.

## Golden Revision
If anything regresses, return to:
- Current golden tag: `GOLDEN_2026-02-10_v27_npu_glint`
- Commit: `git rev-parse GOLDEN_2026-02-10_v27_npu_glint`
- Baseline golden (older): `milestone_raster_flicker_npu_v9` @ `5d569d4352fc723f6d6d567dcdd3c46f58025fd4`

Golden tag policy:
- Golden restore points are updated only on an explicit project-owner directive. See `docs/RESTORE_POINTS.md`.

Failsafe firmware policy:
- A file-based failsafe firmware artifact exists for last-resort board recovery. See `docs/failsafe.md`.

## Last Run
- Date: 2026-02-10
- Result: build + flash ok; LCD shows dune background + shaded "silver ball" with trails; Z scale grows/shrinks on up/down motion and remains latched on abrupt stop; NPU inference enabled and stable
- Binary: `mcuxsdk_ws/build_v28_z_scale_latch_npu/edgeai_sand_demo_cm33_core0.elf`
- Build configuration:
  - `EDGEAI_ENABLE_NPU_INFERENCE=1`
  - `EDGEAI_NPU_BACKEND=1` (Neutron)
- Notes:
  - NPU stack integrated (TFLM + Neutron backend). NPU stepping is compile-time gated (see `EDGEAI_ENABLE_NPU_INFERENCE` in `src/npu_api.h`).
  - Accel debug prints available on VCOM (`/dev/ttyACM0`)
  - Workspace patch applied for upstream TFLM kissfft tool build warning (see `tools/patch_mcuxsdk.sh`)
  - If the ball is stuck, check UART for `EDGEAI: accel ok addr=0x18` (or `0x19`). If “not found” appears, the sensor is not being detected (reseat the clickboard and confirm I2C mode).

## Flash Verification
The last flash was verified against the ELF using LinkServer:
```bash
LinkServer flash auto verify mcuxsdk_ws/build_v26_npu_glint/edgeai_sand_demo_cm33_core0.elf
```

## Background
- Dune background is generated from `downloads/sanddune.jpg` into `src/dune_bg.h` (see `tools/gen_dune_bg.py`).
- Depending on restore point:
  - `milestone_dune_bg_fullscreen_v1`: background is drawn full-screen at boot.
  - `milestone_dune_bg_tile_reveal_v1`: screen boots black and the background “reveals” as tiles are updated while the ball moves.
