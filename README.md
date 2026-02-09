# EdgeAI Sand Demo (FRDM-MCXN947)

This repo is a hardware sandbox for FRDM-MCXN947 + PAR-LCD-S035 + Accel 4 Click (FXLS8974CF over mikroBUS/I2C).

Current demo (known-good):
- Tilt-controlled "silver ball" with shadow and motion trails (480x320).
- NPU is integrated via TFLM + Neutron backend and can modulate the ball's specular "glint".
- Dune background derived from `downloads/sanddune.jpg` (rendered behind the ball).

Current rendering notes:
- The demo uses a single-blit tile renderer by default (`EDGEAI_RENDER_SINGLE_BLIT=1`) to avoid tearing and to keep the background behind the ball.
- Background mode depends on the selected restore point:
  - render full-screen at boot, or
  - start black and “reveal” as the ball moves (dirty-rect tiles only).

## Known-Good Revision (Golden)
If anything breaks, return to this exact revision:
- Current golden tag: `GOLDEN_2026-02-09_v14_docs_sanitize`
- Commit: `git rev-parse GOLDEN_2026-02-09_v14_docs_sanitize`
- Baseline golden (older): `milestone_raster_flicker_npu_v9` @ `5d569d4352fc723f6d6d567dcdd3c46f58025fd4`

Checkout + rebuild + flash:
```bash
git checkout GOLDEN_2026-02-09_v14_docs_sanitize
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
5. Install repo guardrails (optional): `./tools/install_git_hooks.sh`

Serial output (optional):
- `timeout 10 cat /dev/ttyACM0`

## Background Image (Dune)
The background is generated from `downloads/sanddune.jpg` into a low-res heightmap + shaded texture:
- Generated file: `src/dune_bg.h`
- Generator: `tools/gen_dune_bg.py`

Regenerate after changing the source image:
```bash
python3 tools/gen_dune_bg.py --in downloads/sanddune.jpg --out src/dune_bg.h
```

## Background Modes
Two background modes exist as restore points:
- Full-screen at boot: `milestone_dune_bg_fullscreen_v1`
- Tile-only reveal: `milestone_dune_bg_tile_reveal_v1`

For the complete list, see `docs/RESTORE_POINTS.md`.

## Text Style Guardrails
Comments and docs are required to avoid conversational phrasing and direct reader references.
- Rules: `docs/STYLE_RULES.md`
- Lint: `./tools/lint_text_style.sh`

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
