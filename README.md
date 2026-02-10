# EdgeAI Sphere Demo (FRDM-MCXN947)

This repo is a hardware sandbox for FRDM-MCXN947 + PAR-LCD-S035 + Accel 4 Click (FXLS8974CF over mikroBUS/I2C).

Current demo (known-good):
- Tilt-controlled "silver ball" with shadow and motion trails (480x320).
- Third-dimension lift depth cue derived from vertical motion (ball separates from shadow; size/shadow intensity vary).
- NPU is integrated via TFLM + Neutron backend; inference output modulates the ball's specular glint and sparkle intensity.
- Dune background derived from `downloads/sanddune.jpg` (rendered behind the ball).

Current rendering notes:
- The demo uses a single-blit tile renderer by default (`EDGEAI_RENDER_SINGLE_BLIT=1`) to avoid tearing and to keep the background behind the ball.
- Background mode depends on the selected restore point:
  - render full-screen at boot, or
  - start black and “reveal” as the ball moves (dirty-rect tiles only).

## Current Build Status (2026-02-10)
Current restore pointers:
- Golden tag: `GOLDEN_2026-02-10_v27_npu_glint` (see `docs/RESTORE_POINTS.md`)
- Failsafe pointer: `docs/failsafe.md` (see `docs/BUILD_FLASH.md` for flashing)

Functional summary:
- Input: FXLS8974CF accel sample -> `accel_proc` LP/HP outputs
- Simulation: fixed-step ball physics + damping + bounds; optional "bang" impulse
- Depth cue: lift uses a high-pass signal from accel magnitude `|a|` and is smoothed into `ball.lift_q16`
- Rendering: dune background + trails + shadow + reflective ball shader (environment reflection + moving sparkles)
- NPU: when enabled, inference runs periodically and produces a single 0..255 `glint` value that drives specular intensity

## Known-Good Revision (Golden)
If anything breaks, return to this exact revision:
- Current golden tag: `GOLDEN_2026-02-10_v27_npu_glint`
- Commit: `git rev-parse GOLDEN_2026-02-10_v27_npu_glint`
- Baseline golden (older): `milestone_raster_flicker_npu_v9` @ `5d569d4352fc723f6d6d567dcdd3c46f58025fd4`

Golden tag policy:
- Golden restore points are updated only on an explicit project-owner directive. See `docs/RESTORE_POINTS.md`.

Failsafe firmware policy:
- A file-based failsafe firmware artifact exists for last-resort board recovery. See `docs/failsafe.md`.

Checkout + rebuild + flash:
```bash
git checkout GOLDEN_2026-02-10_v27_npu_glint
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

## Tested Host Versions
- Ubuntu: 24.04 LTS
- West: v1.5.0
- Ninja: 1.13.0
- Toolchain: xPack GNU Arm Embedded GCC 14.2.1
- LinkServer: v25.12.83

## Run Validation Checklist
- LCD: boot shows a centered `SAND DUNE` title for ~3 seconds, then transitions to the dune background + ball.
- Motion: ball responds to tilt; trails follow the ball.
- Lift: quick up/down motion produces visible lift (ball rises relative to shadow).
- HUD: top-right text shows `C:###` (FPS) and NPU status.
  - `B:S` stub backend, `B:N` neutron backend
  - `N:1` backend init ok
  - `I:1` NPU step enabled (`EDGEAI_ENABLE_NPU_INFERENCE=1`)

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

## Docs Entry Point
- `docs/START_HERE.md`

## NPU Notes
By default, the firmware initializes the NPU stack but does not run inference (to avoid any platform-specific stalls).
To enable NPU steps, set `EDGEAI_ENABLE_NPU_INFERENCE=1` and rebuild.

Backend selection is compile-time:
- Stub backend: `EDGEAI_NPU_BACKEND=0` (procedural modulation)
- Neutron backend: `EDGEAI_NPU_BACKEND=1` (TFLM + Neutron)

## Tuning / Orientation
Accel axis mapping macros live in `src/accel_proc.h`:
- `EDGEAI_ACCEL_SWAP_XY`
- `EDGEAI_ACCEL_INVERT_X`
- `EDGEAI_ACCEL_INVERT_Y`

Milestone notes:
- `docs/MILESTONE_2026-02-08_TILT_BALL_NPU.md`
