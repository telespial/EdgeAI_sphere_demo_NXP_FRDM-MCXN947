# Build / Flash Notes (FRDM-MCXN947)

## Current Situation In This Workspace
The repo includes helper scripts under `vendors/nxp/mcuxpresso-sdk/` which assume:
- `west` is available
- the MCUX SDK manifest dependencies (including `mcuxsdk/examples/`) are present

This environment currently does **not** have `pip`/`venv` tooling installed, so we cannot install `west` here without OS package install privileges.

## What We Can Still Do
- Implement the portable sim + sensor driver code under `src/`.
- Keep the SDK build entrypoint as an MCUX example, while keeping editable code in this repo.
- When build tooling is available (or on your dev machine), use the existing scripts:
  - `vendors/nxp/mcuxpresso-sdk/build_frdmmcxn947.sh`
  - `vendors/nxp/mcuxpresso-sdk/flash_frdmmcxn947.sh`

## MCUX SDK Layout Expectations
The scripts expect:
- `vendors/nxp/mcuxpresso-sdk/mcuxsdk/` (core SDK) present
- `vendors/nxp/mcuxpresso-sdk/mcuxsdk/examples/` (examples) present
- `vendors/nxp/mcuxpresso-sdk/mcuxsdk/components/` (components) present

## EdgeAI Sand Demo Example Wrapper
Build entrypoint (MCUX example):
- `vendors/nxp/mcuxpresso-sdk/mcuxsdk/examples/demo_apps/edgeai_sand_demo`

Editable code:
- `projects/nxp/frdm-mcxn947/EdgeAI_sand_demo/src/`
- `projects/nxp/frdm-mcxn947/EdgeAI_sand_demo/docs/`

The wrapper example's `CMakeLists.txt` pulls sources/includes from the editable code path.

If you clone this repo on a new machine, install the wrapper into your local `mcuxsdk-examples` checkout with:
```bash
projects/nxp/frdm-mcxn947/EdgeAI_sand_demo/sdk_example/install_mcux_overlay.sh
```

## Flashing
LinkServer is used by default (see `docs/PROJECT_STATE.md` + `docs/OPS_RUNBOOK.md` in top-level `codemaster`).
