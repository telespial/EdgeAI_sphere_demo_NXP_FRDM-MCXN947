# EdgeAI Sand Demo (FRDM-MCXN947)

This repo is a sandbox for an interactive "falling sand" style game on FRDM-MCXN947:
- Simulate sand, water, metal, and a ball.
- Use Accel 4 Click (FXLS8974CF over mikroBUS) to rotate gravity.
- Use the MCXN947 NPU for visual post-processing and/or lightweight ML features.

Working copy:
- `sdk_example/`: SDK-based app project (MCUX SDK / Zephyr-style via `west`)
- `src/`: portable sim + sensor driver code

Build/flash flow:
- Use `scripts/build.sh` and `scripts/flash.sh` from the top-level `codemaster` repo.
- Configure build/flash in `docs/PROJECT_STATE.md` in the top-level `codemaster` repo.

Board docs:
- `platforms/mcu/nxp/mcxn/boards/frdm-mcxn947/docs/` (in the top-level `codemaster` repo)

Status:
- See STATUS.md

Docs (start here):
- `docs/HARDWARE.md`
- `docs/SIM_ARCHITECTURE.md`
- `docs/NPU_PLAN.md`
- `docs/BUILD_FLASH.md`
