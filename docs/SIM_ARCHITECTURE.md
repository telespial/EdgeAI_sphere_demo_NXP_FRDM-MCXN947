# Simulation Architecture (Tilt Ball + Future Falling Sand)

## Current Demo Architecture (Tilt Ball)
The current firmware is a tilt-controlled ball demo with an optional NPU-driven glint modulation signal.

High-level loop (`src/edgeai_sand_demo.c`):
1. Read FXLS8974CF accel sample (I2C).
2. Compute `dt` from `DWT->CYCCNT` with a fixed timestep fallback.
3. Update `accel_proc` filters and compute:
   - `ax_soft_q15`, `ay_soft_q15` (tilt control)
   - bang score/pulse (impulse trigger)
4. Update a latched Z scale command from a high-pass signal of accel magnitude `|a|`.
5. Run fixed-step physics (`sim_step`) at 120 Hz and throttle rendering to ~60 FPS.
6. Draw background-preserving dirty-rect tiles: dune background + trails + shadow + reflective ball + HUD.
7. If inference is enabled, run an NPU step periodically and convert output to `ball.glint` (0..255).

Key modules:
- Orchestration + timing + diagnostics: `src/edgeai_sand_demo.c`
- Accel filtering and event detection: `src/accel_proc.c`, `src/accel_proc.h`
- Physics state and fixed-step update: `src/sim_world.c`, `src/sim_world.h`
- Background/tile renderer + HUD: `src/render_world.c`, `src/render_world.h`
- Software primitives (dune bg, shadow, ball shader): `src/sw_render.c`, `src/sw_render.h`
- NPU backend API: `src/npu_api.c`, `src/npu_api.h`
- Neutron backend wrapper: `src/npu_backend_neutron.cpp`

State representation:
- `ball_state_t` uses Q16 for `x`, `y`, `vx`, `vy`, and `z_scale`:
  - `x_q16`, `y_q16` are pixel position in Q16.
  - `vx_q16`, `vy_q16` are pixels/second in Q16.
  - `z_scale_q16` is a Q16 multiplier in `[EDGEAI_BALL_Z_SCALE_MIN_Q16, EDGEAI_BALL_Z_SCALE_MAX_Q16]`.
- `glint` is a single `uint8_t` (0..255) that modulates specular intensity and sparkle probability.

Timing and stability:
- Physics is fixed-step to avoid velocity damping artifacts at small `dt`.
- Rendering is throttled to a fixed period to keep LCD updates stable.
- A fixed timestep fallback keeps motion predictable if `DWT->CYCCNT` is not advancing.

NPU role (current implementation):
- The Neutron backend reduces model output to a single `glint` value.
- Current input features are synthetic and derived from motion (velocity magnitude) to validate the end-to-end NPU pipeline.

## Future Simulation Architecture (Falling Sand)
The longer-term target is a 2D grid cellular automata for sand/water/metal plus a special "ball" entity.

This is deterministic, fast on MCU, and easy to couple to accelerometer gravity.

## Data Model
- Grid `W x H` of `uint8_t material_id`.
- Optional second grid for per-cell state (future): e.g. water "pressure", temperature, etc.

Suggested material IDs:
- `0` EMPTY
- `1` SAND
- `2` WATER
- `3` METAL (static)
- `4` BALL (either special cells or a separate rigid entity)

## Resolution + Rendering
- Keep sim resolution modest (ex: `160x120`).
- Render via palette (material -> RGB565).
- Scale up to LCD resolution (nearest-neighbor for a crisp pixel-art look).

## Update Loop
At each frame:
1. Read accelerometer `(ax, ay, az)` and low-pass filter.
2. Convert to gravity direction in screen coords.
3. Perform `N` simulation steps.
4. Render to frame buffer.
5. (Optional) NPU post-process (see `NPU_PLAN.md`).
6. Push to display.

## Gravity
Start with 8-way quantized gravity (N, NE, E, SE, S, SW, W, NW) derived from `(ax, ay)`.
This simplifies scan order and move candidates.

## Rules (Initial)
Sand:
- Try move 1 cell along gravity if empty.
- Else try diagonals (gravity+left/right).
- Else stay.

Water:
- Try move along gravity if empty.
- Else diagonals.
- Else spread sideways (perpendicular to gravity), randomized.

Metal:
- No movement.

Ball:
- Phase 1: treat as dense material with a small "momentum" bias.
- Phase 2: rigid circle with `(x, y, vx, vy)` and collision against occupied cells.

## Determinism + Visual Quality
- Use a tiny PRNG (xorshift) for tie-breaking.
- Scan order must depend on gravity so particles don't "fight" the direction.
