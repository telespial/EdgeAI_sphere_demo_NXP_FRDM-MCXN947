#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    int32_t ax_lp;
    int32_t ay_lp;
    int32_t az_lp;
    bool bang_prev;
} accel_proc_t;

typedef struct
{
    int32_t ax_lp;
    int32_t ay_lp;
    int32_t az_lp;
    int32_t ax_hp;
    int32_t ay_hp;
    int32_t az_hp;
    int32_t bang_score;
    int32_t ax_soft_q15;
    int32_t ay_soft_q15;
    bool bang_pulse;
} accel_proc_out_t;

void accel_proc_init(accel_proc_t *s);

/* Axis mapping configuration (compile-time). */
#ifndef EDGEAI_ACCEL_SWAP_XY
#define EDGEAI_ACCEL_SWAP_XY 1
#endif
#ifndef EDGEAI_ACCEL_INVERT_X
#define EDGEAI_ACCEL_INVERT_X 0
#endif
#ifndef EDGEAI_ACCEL_INVERT_Y
#define EDGEAI_ACCEL_INVERT_Y 0
#endif

void accel_proc_apply_axis_map(int32_t *ax, int32_t *ay);

/* Converts raw 12-bit accel sample into a smoothed, deadzoned, soft-response vector.
 * Output is in Q15, approximately normalized to [-1, 1] around 1g.
 */
void accel_proc_update(accel_proc_t *s, int32_t raw_x, int32_t raw_y, int32_t raw_z, accel_proc_out_t *out);
