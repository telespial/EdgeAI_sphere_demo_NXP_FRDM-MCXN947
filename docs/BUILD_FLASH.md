# Build / Flash Notes (FRDM-MCXN947)

## Current Situation In This Workspace
The repo includes helper scripts under `vendors/nxp/mcuxpresso-sdk/` which assume:
- `west` is available
- the MCUX SDK manifest dependencies (including `mcuxsdk/examples/`) are present

This environment currently does **not** have `pip`/`venv` tooling installed, so we cannot install `west` here without OS package install privileges.

Also, the upstream MCUX SDK uses `west` manifests to populate additional repos (notably `mcuxsdk/examples/`). We checked out `mcuxsdk/components/` (per manifest), but `mcuxsdk/examples/` is still missing in this workspace.

## What We Can Still Do
- Implement the portable sim + sensor driver code under `src/`.
- Keep the SDK project in `sdk_example/` aligned with the known-good LVGL example structure.
- When build tooling is available (or on your dev machine), use the existing scripts:
  - `vendors/nxp/mcuxpresso-sdk/build_frdmmcxn947.sh`
  - `vendors/nxp/mcuxpresso-sdk/flash_frdmmcxn947.sh`

## MCUX SDK Layout Expectations
The scripts expect:
- `vendors/nxp/mcuxpresso-sdk/mcuxsdk/` (core SDK) present
- `vendors/nxp/mcuxpresso-sdk/mcuxsdk/examples/` (examples) present
- `vendors/nxp/mcuxpresso-sdk/mcuxsdk/components/` (components) present

## Flashing
LinkServer is used by default (see `docs/PROJECT_STATE.md` + `docs/OPS_RUNBOOK.md` in top-level `codemaster`).
