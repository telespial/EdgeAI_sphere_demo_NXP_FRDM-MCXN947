# EdgeAI Sphere Demo (FRDM-MCXN947)

This MCUX SDK example wrapper builds the app sources from the repo root (`src/` and `docs/`) via the overlay in `sdk_example/`.

## Current Build Status (2026-02-10)
- Golden: `GOLDEN_2026-02-10_v27_npu_glint`
- Last verified flash artifact: `mcuxsdk_ws/build_v26_npu_glint/edgeai_sand_demo_cm33_core0.elf`

Runtime behavior:
- Dune background with a reflective silver ball (shadow + trails).
- Lift depth cue driven by vertical motion (HP of accel magnitude).
- HUD shows FPS and NPU status.
- Optional NPU inference (Neutron backend) generates a 0..255 `glint` modulation value used by the ball shader.

## Hardware Assumptions
- FRDM-MCXN947
- PAR-LCD-S035 (ST7796S over FlexIO 8080), `480x320`
- MikroShield + Accel 4 Click (FXLS8974CF) on mikroBUS
  - I2C via FC3 (mikroBUS SDA/SCL), address `0x18` or `0x19`, `WHO_AM_I` = `0x86`

## Notes
- The app name in MCUX remains `edgeai_sand_demo` for SDK integration convenience; the repo-level project name is "EdgeAI Sphere Demo".
