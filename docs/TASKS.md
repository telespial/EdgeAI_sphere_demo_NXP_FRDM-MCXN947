# Tasks

## Current Build Status (2026-02-10)
- Golden: `GOLDEN_2026-02-10_v27_npu_glint`
- Last verified flash artifact: `mcuxsdk_ws/build_v26_npu_glint/edgeai_sand_demo_cm33_core0.elf`
- Primary behavior: dune background + reflective silver ball with trails/shadow, lift depth cue, and NPU-driven `glint` modulation.

## Run Validation (Hardware)
1. Build the golden profile (NPU inference enabled, Neutron backend) and flash.
2. Confirm boot title, background render, and ball visibility.
3. Confirm tilt controls X/Y and trails follow motion.
4. Confirm lift responds to vertical motion (ball separates from shadow).
5. Confirm HUD indicates backend/init/inference state (`B:N N:1 I:1` for the golden profile).
6. Optional: confirm VCOM logs (`EDGEAI:` banner + per-second stats) are present.

## Bring-up (Accel)
1. Confirm I2C address (`0x18` or `0x19`) by reading `WHO_AM_I` (expect `0x86`).
2. Set range (FSR) and ODR.
3. Read XYZ at a fixed rate and validate orientation and axis mapping.
4. Optional: enable INT1 data-ready for stable sampling.

## NPU Glint (Current)
1. Confirm `EDGEAI_ENABLE_NPU_INFERENCE=1` and `EDGEAI_NPU_BACKEND=1` are set in the build.
2. Confirm `edgeai_npu_init()` succeeds (`N:1` in HUD) and inference is enabled (`I:1`).
3. Confirm `glint` changes during motion and modulates specular intensity/sparkles.

## NPU Post-Process (Next)
1. Add a low-res scene buffer and a CNN post-process stage (see `docs/NPU_PLAN.md`).
2. Keep a stub backend path for deterministic A/B comparisons.
3. Preserve the current render loop timing and avoid regressions in ball visibility.

## Falling Sand (Future)
1. Implement grid materials and update rules.
2. Couple gravity to accel tilt.
3. Render palette and scale to LCD.
4. Optional: NPU-assisted post-process and surrogate physics.
