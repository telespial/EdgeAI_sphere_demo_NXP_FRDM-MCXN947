#pragma once

#include <stdint.h>

typedef enum {
  MAT_EMPTY = 0,
  MAT_SAND  = 1,
  MAT_WATER = 2,
  MAT_METAL = 3,
  MAT_BALL  = 4,
} material_t;

typedef enum {
  GRAV_N  = 0,
  GRAV_NE = 1,
  GRAV_E  = 2,
  GRAV_SE = 3,
  GRAV_S  = 4,
  GRAV_SW = 5,
  GRAV_W  = 6,
  GRAV_NW = 7,
} grav_dir_t;

typedef struct {
  uint16_t w;
  uint16_t h;
  uint8_t *cells; // size = w*h
} sim_grid_t;

// Deterministic PRNG for tie-breaks (xorshift32).
typedef struct {
  uint32_t s;
} sim_rng_t;

void sim_rng_seed(sim_rng_t *rng, uint32_t seed);
uint32_t sim_rng_u32(sim_rng_t *rng);

void sim_step(sim_grid_t *g, grav_dir_t dir, sim_rng_t *rng);
void sim_clear(sim_grid_t *g);
void sim_set(sim_grid_t *g, uint16_t x, uint16_t y, material_t m);
material_t sim_get(const sim_grid_t *g, uint16_t x, uint16_t y);

