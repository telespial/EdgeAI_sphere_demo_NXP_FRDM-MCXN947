# TODO.md — EdgeAI Sphere Demo Next Steps (NPU + 3D Controls)

This file tracks the next features beyond the v23 ball polish baseline.

## Current Status (2026-02-10)
- Golden: `GOLDEN_2026-02-10_v27_npu_glint`
- Last verified flash artifact: `mcuxsdk_ws/build_v26_npu_glint/edgeai_sand_demo_cm33_core0.elf`
- Implemented features:
  - Tilt-controlled ball with trails + shadow and reflective shader (env reflection + sparkles).
  - Lift depth cue driven by vertical motion (HP of accel magnitude).
  - NPU inference (Neutron backend) enabled in the last verified build; output drives a single `glint` modulation signal.
- Remaining scope in this file focuses on moving from "glint modulation" to a real post-process pipeline and gesture/surrogate features.

Execution order:
1. D1: Third dimension control (board raise/lower -> ball lift) (implemented; keep for regression checks)
2. N1: Phase 1 NPU post-process (low-res CNN + composite)
3. N2: Gesture classifier (accel time-series)
4. N3: Surrogate physics (model-assisted sand/water/material updates)

---

## D1) Third Dimension Control (HIGH)

Goal:
- Add a third axis of control so board raise/lower maps to an on-screen "lift" dimension for the ball.
- Lift is primarily a visual depth cue (ball moves relative to its shadow); 2D motion remains tilt-driven.
  - Note: the accelerometer does not measure absolute height; this control reacts to up/down acceleration.

Implementation sketch:
- Add `lift_q16` to `ball_state_t` and update it from a processed Z-axis signal (`az_lp` or derived).
- Render ball at `(x, y - lift_px)` while keeping shadow/trails anchored at `(x, y)`.
- Keep the dirty-rect tile approach; ensure lift stays within padding limits.
- Optional polish:
  - shadow size/intensity varies with lift
  - lift value is visible in UART debug output

Acceptance criteria:
- On hardware, raising/lowering the board (up/down motion) causes visible lift changes (ball separates from its shadow).
- No stuck pixels or tearing regressions.
- Build + flash remain repeatable; text style lint passes.

Current implementation notes:
- Lift uses a high-pass signal from accel magnitude `|a|` to avoid relying on board orientation.
- `lift_target_q16` is smoothed into `ball.lift_q16` in `sim_step()`.
- Rendering uses `cy_draw = cy_ground - lift_px` and adjusts ball radius and shadow alpha.

---

## N1) Phase 1 NPU Post-Process (HIGH)

Goal:
- Run a small fixed-weight CNN on a low-res buffer (example: `160x120`) and composite the result into the main render.
- Target effects: edge/glow for the ball, water shimmer, sand stylization, trail enhancement.

Implementation sketch:
- Define a low-res input buffer representing the scene (luminance, height, material ID, or stacked channels).
- Add a post-process output buffer and a CPU composite step.
- Keep `EDGEAI_ENABLE_NPU_INFERENCE` as a hard gate.
- Stub backend provides a deterministic placeholder effect for comparison.

Acceptance criteria:
- With inference enabled, an obvious visual effect appears and remains stable across minutes of runtime.
- With inference disabled or stub backend selected, behavior remains stable and predictable.

---

## N2) Gesture Classifier (Optional, MED)

Goal:
- Detect tap/shake/swirl gestures from accelerometer time-series data.
- Gesture path is separate from the current CPU "bang" impulse and can drive events or visual modes.

Implementation sketch:
- Maintain a short rolling window of accel samples (mapped axes, with LP/HP available).
- Implement a CPU baseline classifier (threshold + features) first.
- Optionally replace with a small NPU model once stability is confirmed.

Acceptance criteria:
- Tap/shake/swirl detection is reliable on hardware and does not interfere with tilt control.
- False-positive rate remains low during normal tilt gameplay.

---

## N3) Surrogate Physics (Optional, MED)

Goal:
- Use a model-assisted update for sand/water/material rules as complexity grows.

Implementation sketch:
- Define a compact patch representation (local neighborhood + gravity vector).
- Prototype offline training and a small on-device model.
- Keep deterministic CPU physics as a fallback and for debug comparisons.

Acceptance criteria:
- Model-assisted path matches baseline behavior within an acceptable error band.
- Fallback path remains available and easy to enable.
