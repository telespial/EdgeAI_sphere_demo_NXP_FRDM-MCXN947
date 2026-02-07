#include "sim.h"

#include <stdbool.h>
#include <string.h>

static uint32_t xorshift32(uint32_t x) {
  // xorshift32: simple deterministic PRNG for embedded use.
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}

void sim_rng_seed(sim_rng_t *rng, uint32_t seed) {
  if (!rng) return;
  rng->s = seed ? seed : 0x12345678u;
}

uint32_t sim_rng_u32(sim_rng_t *rng) {
  if (!rng) return 0;
  rng->s = xorshift32(rng->s);
  return rng->s;
}

void sim_clear(sim_grid_t *g) {
  if (!g || !g->cells) return;
  memset(g->cells, 0, (size_t)g->w * (size_t)g->h);
}

void sim_set(sim_grid_t *g, uint16_t x, uint16_t y, material_t m) {
  if (!g || !g->cells) return;
  if (x >= g->w || y >= g->h) return;
  g->cells[(size_t)y * g->w + x] = (uint8_t)m;
}

material_t sim_get(const sim_grid_t *g, uint16_t x, uint16_t y) {
  if (!g || !g->cells) return MAT_EMPTY;
  if (x >= g->w || y >= g->h) return MAT_EMPTY;
  return (material_t)g->cells[(size_t)y * g->w + x];
}

static bool in_bounds(const sim_grid_t *g, int x, int y) {
  return g && g->cells && x >= 0 && y >= 0 && x < (int)g->w && y < (int)g->h;
}

static void swap_cells(sim_grid_t *g, int x0, int y0, int x1, int y1) {
  uint8_t *a = &g->cells[(size_t)y0 * g->w + (size_t)x0];
  uint8_t *b = &g->cells[(size_t)y1 * g->w + (size_t)x1];
  uint8_t t = *a;
  *a = *b;
  *b = t;
}

static void grav_vectors(grav_dir_t dir, int *gx, int *gy, int *sx, int *sy) {
  // gx,gy = primary gravity step
  // sx,sy = sideways step (perpendicular-ish) used for water spread
  switch (dir) {
    case GRAV_N:  *gx = 0; *gy = -1; *sx = 1; *sy = 0; break;
    case GRAV_NE: *gx = 1; *gy = -1; *sx = 1; *sy = 1; break;
    case GRAV_E:  *gx = 1; *gy = 0; *sx = 0; *sy = 1; break;
    case GRAV_SE: *gx = 1; *gy = 1; *sx = -1; *sy = 1; break;
    case GRAV_S:  *gx = 0; *gy = 1; *sx = 1; *sy = 0; break;
    case GRAV_SW: *gx = -1; *gy = 1; *sx = -1; *sy = -1; break;
    case GRAV_W:  *gx = -1; *gy = 0; *sx = 0; *sy = 1; break;
    case GRAV_NW: *gx = -1; *gy = -1; *sx = 1; *sy = -1; break;
    default:      *gx = 0; *gy = 1; *sx = 1; *sy = 0; break;
  }
}

static void scan_order(const sim_grid_t *g, int gx, int gy, int *x0, int *x1, int *xstep,
                       int *y0, int *y1, int *ystep) {
  // Choose scan order opposite gravity so particles fall "into" unprocessed cells.
  if (gx >= 0) { *x0 = (int)g->w - 1; *x1 = -1; *xstep = -1; }
  else         { *x0 = 0;            *x1 = (int)g->w; *xstep = 1;  }

  if (gy >= 0) { *y0 = (int)g->h - 1; *y1 = -1; *ystep = -1; }
  else         { *y0 = 0;            *y1 = (int)g->h; *ystep = 1;  }
}

static void step_sand(sim_grid_t *g, int x, int y, int gx, int gy, sim_rng_t *rng) {
  int nx = x + gx;
  int ny = y + gy;
  if (in_bounds(g, nx, ny) && sim_get(g, (uint16_t)nx, (uint16_t)ny) == MAT_EMPTY) {
    swap_cells(g, x, y, nx, ny);
    return;
  }
  /* Sand sinks through water. */
  if (in_bounds(g, nx, ny) && sim_get(g, (uint16_t)nx, (uint16_t)ny) == MAT_WATER) {
    swap_cells(g, x, y, nx, ny);
    return;
  }

  // Try diagonals around gravity.
  int dx0 = gx + ((gy != 0) ? 1 : 0);
  int dy0 = gy + ((gx != 0) ? 1 : 0);
  int dx1 = gx - ((gy != 0) ? 1 : 0);
  int dy1 = gy - ((gx != 0) ? 1 : 0);

  // Randomize diagonal preference.
  if (rng && (sim_rng_u32(rng) & 1u)) {
    int tx = dx0; int ty = dy0;
    dx0 = dx1; dy0 = dy1;
    dx1 = tx;  dy1 = ty;
  }

  nx = x + dx0; ny = y + dy0;
  if (in_bounds(g, nx, ny) && sim_get(g, (uint16_t)nx, (uint16_t)ny) == MAT_EMPTY) {
    swap_cells(g, x, y, nx, ny);
    return;
  }
  if (in_bounds(g, nx, ny) && sim_get(g, (uint16_t)nx, (uint16_t)ny) == MAT_WATER) {
    swap_cells(g, x, y, nx, ny);
    return;
  }
  nx = x + dx1; ny = y + dy1;
  if (in_bounds(g, nx, ny) && sim_get(g, (uint16_t)nx, (uint16_t)ny) == MAT_EMPTY) {
    swap_cells(g, x, y, nx, ny);
    return;
  }
  if (in_bounds(g, nx, ny) && sim_get(g, (uint16_t)nx, (uint16_t)ny) == MAT_WATER) {
    swap_cells(g, x, y, nx, ny);
    return;
  }
}

static void step_water(sim_grid_t *g, int x, int y, int gx, int gy, int sx, int sy, sim_rng_t *rng) {
  int nx = x + gx;
  int ny = y + gy;
  if (in_bounds(g, nx, ny) && sim_get(g, (uint16_t)nx, (uint16_t)ny) == MAT_EMPTY) {
    swap_cells(g, x, y, nx, ny);
    return;
  }

  // Diagonals similar to sand.
  int dx0 = gx + ((gy != 0) ? 1 : 0);
  int dy0 = gy + ((gx != 0) ? 1 : 0);
  int dx1 = gx - ((gy != 0) ? 1 : 0);
  int dy1 = gy - ((gx != 0) ? 1 : 0);
  if (rng && (sim_rng_u32(rng) & 1u)) {
    int tx = dx0; int ty = dy0;
    dx0 = dx1; dy0 = dy1;
    dx1 = tx;  dy1 = ty;
  }
  nx = x + dx0; ny = y + dy0;
  if (in_bounds(g, nx, ny) && sim_get(g, (uint16_t)nx, (uint16_t)ny) == MAT_EMPTY) {
    swap_cells(g, x, y, nx, ny);
    return;
  }
  nx = x + dx1; ny = y + dy1;
  if (in_bounds(g, nx, ny) && sim_get(g, (uint16_t)nx, (uint16_t)ny) == MAT_EMPTY) {
    swap_cells(g, x, y, nx, ny);
    return;
  }

  // Spread sideways.
  int s0x = sx, s0y = sy;
  int s1x = -sx, s1y = -sy;
  if (rng && (sim_rng_u32(rng) & 1u)) {
    int tx = s0x; int ty = s0y;
    s0x = s1x; s0y = s1y;
    s1x = tx;  s1y = ty;
  }

  /* Try to flow sideways up to a few cells to look less "stuck". */
  for (int dist = 1; dist <= 3; dist++) {
    nx = x + s0x * dist; ny = y + s0y * dist;
    if (in_bounds(g, nx, ny) && sim_get(g, (uint16_t)nx, (uint16_t)ny) == MAT_EMPTY) {
      swap_cells(g, x, y, nx, ny);
      return;
    }
  }
  for (int dist = 1; dist <= 3; dist++) {
    nx = x + s1x * dist; ny = y + s1y * dist;
    if (in_bounds(g, nx, ny) && sim_get(g, (uint16_t)nx, (uint16_t)ny) == MAT_EMPTY) {
      swap_cells(g, x, y, nx, ny);
      return;
    }
  }
}

void sim_step(sim_grid_t *g, grav_dir_t dir, sim_rng_t *rng) {
  if (!g || !g->cells || g->w == 0 || g->h == 0) return;

  int gx = 0, gy = 1, sx = 1, sy = 0;
  grav_vectors(dir, &gx, &gy, &sx, &sy);

  int x0, x1, xstep, y0, y1, ystep;
  scan_order(g, gx, gy, &x0, &x1, &xstep, &y0, &y1, &ystep);

  for (int y = y0; y != y1; y += ystep) {
    for (int x = x0; x != x1; x += xstep) {
      material_t m = sim_get(g, (uint16_t)x, (uint16_t)y);
      switch (m) {
        case MAT_SAND:
          step_sand(g, x, y, gx, gy, rng);
          break;
        case MAT_WATER:
          step_water(g, x, y, gx, gy, sx, sy, rng);
          break;
        case MAT_EMPTY:
        case MAT_METAL:
        case MAT_BALL:
        default:
          break;
      }
    }
  }
}
