# Simulation Architecture (Falling Sand)

## Core Idea
Use a 2D grid cellular automata for sand/water/metal and a special "ball" entity.

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

