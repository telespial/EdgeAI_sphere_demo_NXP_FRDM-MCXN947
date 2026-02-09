#pragma once

#include <stdint.h>

typedef struct
{
    int32_t x_q16;
    int32_t y_q16;
    int32_t vx_q16;
    int32_t vy_q16;
    uint8_t glint;
} ball_state_t;

typedef struct
{
    ball_state_t ball;
} sim_world_t;

typedef struct
{
    int32_t ax_soft_q15;
    int32_t ay_soft_q15;
    int32_t bang_dvx_q16;
    int32_t bang_dvy_q16;
} sim_input_t;

typedef struct
{
    int32_t sim_step_q16;
    int32_t a_px_s2;
    int32_t damp_q16;
    int32_t minx;
    int32_t miny;
    int32_t maxx;
    int32_t maxy;
} sim_params_t;

void sim_world_init(sim_world_t *w, int32_t lcd_w, int32_t lcd_h);
void sim_step(sim_world_t *w, const sim_input_t *in, const sim_params_t *p);
