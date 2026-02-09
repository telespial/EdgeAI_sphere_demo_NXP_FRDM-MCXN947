# TODO.md — EdgeAI Sphere Demo Next Steps (NPU + 3D Controls)

This file tracks the next features beyond the v25 golden baseline.

Current golden restore point:
- `GOLDEN_2026-02-09_v25_roll_speed_limit`

Execution order:
1. N1: Phase 1 NPU post-process (low-res CNN + composite)
2. N2: Gesture classifier (accel time-series)
3. N3: Surrogate physics (model-assisted sand/water/material updates)

---

## D1) Third Dimension Control (DONE)

Status:
- Implemented as a vertical-motion depth cue (lift, size, and shadow intensity modulation).

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

---

## N1) Phase 1 NPU Post-Process (HIGH)

Goal:
- Run a small fixed-weight CNN on a low-res buffer (example: `160x120`) and composite the result into the main render.
- Target effects: edge/glow for the ball, water shimmer, sand stylization, trail enhancement.
  - Visual priority: a more realistic reflective ball bearing (environment reflections + moving shimmer).

Implementation sketch:
- Define a low-res input buffer representing the scene (luminance, height, material ID, or stacked channels).
- Add a post-process output buffer and a CPU composite step.
- Keep `EDGEAI_ENABLE_NPU_INFERENCE` as a hard gate.
- Stub backend provides a deterministic placeholder effect for comparison.
  - First milestone: CPU fixed-weight conv stack (edge -> blur/glow) wired through the same API shape intended for NPU.

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
