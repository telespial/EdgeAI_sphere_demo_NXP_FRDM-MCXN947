#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sim_world.h"

typedef struct
{
    enum { EDGEAI_TRAIL_N = 12 } _dummy_enum;
    int16_t trail_x[EDGEAI_TRAIL_N];
    int16_t trail_y[EDGEAI_TRAIL_N];
    uint32_t trail_head;
    int32_t prev_x;
    int32_t prev_y; /* previous cy_draw */
    int32_t prev_cy_ground;
    int32_t prev_r_draw;
    int32_t prev_r_shadow;
    uint32_t frame;
} render_state_t;

typedef struct
{
    bool accel_fail;
    uint32_t fps_last;
    bool npu_init_ok;
    bool npu_run_enabled;
    char npu_backend;
} render_hud_t;

void render_world_init(render_state_t *rs, int32_t cx, int32_t cy);

/* Full-screen background draw (tiled to fit EDGEAI_TILE_MAX_*). */
void render_world_draw_full_background(void);

/* Renders one frame if do_render is true. Returns true when a draw was issued. */
bool render_world_draw(render_state_t *rs,
                       const sim_world_t *world,
                       bool do_render,
                       const render_hud_t *hud);
