# Start Here

## Current Build Status (2026-02-10)
- Golden restore tag: `GOLDEN_2026-02-10_v27_npu_glint`
- Last verified flash artifact: `mcuxsdk_ws/build_v28_z_scale_latch_npu/edgeai_sand_demo_cm33_core0.elf`
- Failsafe pointer: `docs/failsafe.md` (flash requires explicit filename confirmation)

Behavior summary:
- Tilt-controlled reflective silver ball over a dune background with trails + shadow.
- Z scale latch driven by vertical motion (HP of accel magnitude): ball grows/shrinks and holds last scale.
- NPU inference (Neutron backend) runs periodically and modulates specular glint intensity.

Read in order:
1. `docs/HARDWARE.md`
2. `docs/BUILD_FLASH.md`
3. `docs/ACCEL_BRINGUP.md`
4. `docs/SIM_ARCHITECTURE.md`
5. `docs/NPU_PLAN.md`
6. `docs/RESTORE_POINTS.md`
7. `docs/failsafe.md`

Policy notes:
- Golden restore points are updated only on an explicit project-owner directive. See `docs/RESTORE_POINTS.md`.
- The failsafe firmware pointer is updated only on an explicit project-owner directive. See `docs/RESTORE_POINTS.md`.
