# EdgeAI Sand Demo (FRDM-MCXN947)

This repo is a hardware sandbox for FRDM-MCXN947 + PAR-LCD-S035 + Accel 4 Click (FXLS8974CF over mikroBUS/I2C).

Current demo (known-good):
- Tilt-controlled "silver ball" with shadow and motion trails (480x320).
- NPU is integrated via TFLM + Neutron backend and can modulate the ball's specular "glint".

## Known-Good Revision (Golden)
If anything breaks, return to this exact revision:
- Tag: `milestone_raster_flicker_npu_v9`
- Commit: `5d569d4352fc723f6d6d567dcdd3c46f58025fd4`

Checkout + rebuild + flash:
```bash
git checkout milestone_raster_flicker_npu_v9
MCUX_EXAMPLES_DIR="$PWD/mcuxsdk_ws_test/mcuxsdk/examples" ./sdk_example/install_mcux_overlay.sh
ninja -C mcuxsdk_ws_test/build
WS_DIR="$PWD/mcuxsdk_ws_test" ./tools/flash_frdmmcxn947.sh
```

Key folders:
- `sdk_example/`: MCUX SDK example wrapper (built via `west`)
- `src/`: app + display + accel + NPU wrapper
- `tools/`: bootstrap/build/flash scripts

## Quickstart (Ubuntu)
1. Bootstrap user-local tools (no sudo): `./tools/bootstrap_ubuntu_user.sh`
2. Create/update local MCUX west workspace: `./tools/setup_mcuxsdk_ws.sh`
3. Build: `./tools/build_frdmmcxn947.sh debug`
4. Flash (requires NXP LinkServer installed): `./tools/flash_frdmmcxn947.sh`

Serial output (optional):
- `timeout 10 cat /dev/ttyACM0`

## NPU Notes
By default, the firmware initializes the NPU stack but does not run inference (to avoid any platform-specific stalls).
To enable inference, set `EDGEAI_ENABLE_NPU_INFERENCE=1` in `src/edgeai_sand_demo.c` and rebuild.

## Tuning / Orientation
Accel axis mapping macros live in `src/edgeai_sand_demo.c`:
- `EDGEAI_ACCEL_SWAP_XY`
- `EDGEAI_ACCEL_INVERT_X`
- `EDGEAI_ACCEL_INVERT_Y`

Milestone notes:
- `docs/MILESTONE_2026-02-08_TILT_BALL_NPU.md`
