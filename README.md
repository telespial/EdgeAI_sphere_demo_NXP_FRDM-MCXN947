# EdgeAI Sand Demo (FRDM-MCXN947)

This repo is a sandbox for an interactive "falling sand" style game on FRDM-MCXN947:
- Simulate sand, water, metal, and a ball.
- Use Accel 4 Click (FXLS8974CF over mikroBUS) to rotate gravity.
- Use the MCXN947 NPU for visual post-processing and/or lightweight ML features.

Working copy:
- `sdk_example/`: SDK-based app project (MCUX SDK / Zephyr-style via `west`)
- `src/`: portable sim + sensor driver code

Build/flash flow (self-contained):
1. Bootstrap user-local tools (no sudo): `./tools/bootstrap_ubuntu_user.sh`
2. Create/update local MCUX west workspace: `./tools/setup_mcuxsdk_ws.sh`
   - If you hit a transient network/TLS error, just re-run the setup script.
3. Build: `./tools/build_frdmmcxn947.sh`
4. Flash (requires NXP LinkServer installed): `./tools/flash_frdmmcxn947.sh`

Board docs:
- See `docs/HARDWARE.md`

Status:
- See STATUS.md

Docs (start here):
- `docs/HARDWARE.md`
- `docs/SIM_ARCHITECTURE.md`
- `docs/NPU_PLAN.md`
- `docs/BUILD_FLASH.md`
