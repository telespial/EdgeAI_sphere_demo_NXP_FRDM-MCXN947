# Milestone: 2026-02-08 Tilt Ball + NPU Glint (FRDM-MCXN947)

This document records the "known-good" interactive moment:
- LCD: PAR-LCD-S035 (ST7796S over FlexIO 8080) @ `480x320`
- Accel: Accel 4 Click (FXLS8974CF) on mikroBUS, assumed I2C @ 400 kHz
- Visual: shaded silver ball with shadow + trails, smooth tilt response
- NPU: eIQ Neutron NPU used via TFLM Neutron backend; model output modulates specular glint

## Golden Revision Pointer
Return here if anything breaks:
- Tag: `milestone_raster_flicker_npu_v9`
- Commit: `5d569d4352fc723f6d6d567dcdd3c46f58025fd4`

## Build + Flash
From repo root:
```bash
./tools/bootstrap_ubuntu_user.sh
./tools/setup_mcuxsdk_ws.sh
./tools/build_frdmmcxn947.sh debug
./tools/flash_frdmmcxn947.sh
```

Notes:
- `tools/build_frdmmcxn947.sh` runs `tools/patch_mcuxsdk.sh` to keep upstream MCUX SDK builds reproducible.
- If you use a non-default workspace, set `WS_DIR`:
```bash
WS_DIR="$PWD/mcuxsdk_ws_test" ./tools/build_frdmmcxn947.sh debug
WS_DIR="$PWD/mcuxsdk_ws_test" ./tools/flash_frdmmcxn947.sh
```

## Serial (Optional)
VCOM typically enumerates as `/dev/ttyACM0`:
```bash
timeout 10 cat /dev/ttyACM0
```

## Where The Behavior Lives
- Physics + accel mapping + NPU hook: `src/edgeai_sand_demo.c`
- Display primitives + silver shading: `src/par_lcd_s035.c`
- Accel driver (FXLS8974CF): `src/fxls8974cf.c`
- NPU/TFLM wrapper: `src/npu/model.h`, `src/npu/model.cpp`, `src/npu/model_ops_npu.cpp`, `src/npu/model_data.h`

## Key Constants (Snapshot)
In `src/edgeai_sand_demo.c`:
- `LCD_W=480`, `LCD_H=320`
- `BALL_R=20`
- `ACCEL_MAP_DENOM=512` (empirical: ~512 counts per 1g for current FXLS8974 config)
- Physics runs fixed-step at `120 Hz` (accumulator) and renders at ~`60 FPS` (throttled).
- Tilt gain: `a_px_s2=4200` (effective, after the arcade response curve)
- NPU tick period: `200ms` (glint update when inference is enabled)

## Timestep / "Frozen Ball" Guard
The physics integrator uses `DWT->CYCCNT` when available, but falls back to a fixed ~60 FPS timestep if `DWT->CYCCNT` is not advancing (which would otherwise produce `dt==0` and freeze motion).

## Orientation / Axis Mapping
If tilt directions feel wrong, adjust these macros in `src/edgeai_sand_demo.c`:
- `EDGEAI_ACCEL_SWAP_XY`
- `EDGEAI_ACCEL_INVERT_X`
- `EDGEAI_ACCEL_INVERT_Y`

## What "NPU In The Loop" Means Here
This milestone runs a Neutron-backed TFLM model periodically and converts its output to a `glint` value (0..255) used by the renderer to boost specular highlights.

The current input tensor is synthetic (derived from motion/tilt) to validate end-to-end plumbing and performance. Replace the input-features later with real signals (e.g., accel history, gesture features, simulated fluid state tiles).

Note: `EDGEAI_ENABLE_NPU_INFERENCE` is currently `0` by default to avoid platform-specific stalls during development.
