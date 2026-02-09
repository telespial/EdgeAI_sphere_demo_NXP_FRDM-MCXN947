#pragma once

#include <stdint.h>

/* Display geometry. */
#define EDGEAI_LCD_W 480
#define EDGEAI_LCD_H 320

/* Ball radius tuning.
 * Depth cue: smaller toward the top ("far"), larger toward the bottom ("near").
 */
#define EDGEAI_BALL_R_MIN 12
#define EDGEAI_BALL_R_MAX 34

/* Perspective radius for the ball, derived from its center y coordinate. */
static inline int32_t edgeai_ball_r_for_y(int32_t cy)
{
    const int32_t y_far = 26;
    const int32_t y_near = EDGEAI_LCD_H - 26;
    int32_t denom = (y_near - y_far);
    int32_t t_q8 = 256;
    if (denom > 0)
    {
        int32_t num = cy - y_far;
        if (num < 0) num = 0;
        if (num > denom) num = denom;
        t_q8 = (num * 256) / denom;
    }

    int32_t r = EDGEAI_BALL_R_MIN + ((t_q8 * (EDGEAI_BALL_R_MAX - EDGEAI_BALL_R_MIN)) / 256);
    if (r < EDGEAI_BALL_R_MIN) r = EDGEAI_BALL_R_MIN;
    if (r > EDGEAI_BALL_R_MAX) r = EDGEAI_BALL_R_MAX;
    return r;
}

/* Accelerometer normalization.
 * FXLS8974 configuration yields roughly ~512 counts per 1g in the current mode.
 */
#define EDGEAI_ACCEL_MAP_DENOM 512

/* Impact ("bang") detection tuning.
 * Uses a high-pass term: hp = raw - low-pass(raw), in raw sensor counts.
 */
#ifndef EDGEAI_BANG_THRESHOLD
#define EDGEAI_BANG_THRESHOLD 220
#endif

/* Max velocity impulse (px/s, Q16) for a strong bang (>= ~1g over threshold). */
#ifndef EDGEAI_BANG_GAIN_Q16
#define EDGEAI_BANG_GAIN_Q16 (250 << 16)
#endif

/* Render tile limits (single-blit path). */
#define EDGEAI_TILE_MAX_W 200
#define EDGEAI_TILE_MAX_H 200
