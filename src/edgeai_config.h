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

/* Ball lift (third dimension control).
 * Lift is a visual depth cue (ball moves relative to its shadow) derived from vertical motion.
 * Note: an accelerometer cannot measure absolute height; this reacts to up/down acceleration.
 * Keep `EDGEAI_BALL_LIFT_MAX_PX` within the renderer padding budget.
 */
#ifndef EDGEAI_BALL_LIFT_MAX_PX
#define EDGEAI_BALL_LIFT_MAX_PX 28
#endif
#ifndef EDGEAI_BALL_LIFT_SMOOTH_SHIFT
#define EDGEAI_BALL_LIFT_SMOOTH_SHIFT 3
#endif
/* Baseline and mapping for accel magnitude (counts). */
#ifndef EDGEAI_BALL_LIFT_GMAG_LP_SHIFT
#define EDGEAI_BALL_LIFT_GMAG_LP_SHIFT 6
#endif
#ifndef EDGEAI_BALL_LIFT_GMAG_RANGE
#define EDGEAI_BALL_LIFT_GMAG_RANGE 80
#endif
#ifndef EDGEAI_BALL_LIFT_GMAG_DEADZONE
#define EDGEAI_BALL_LIFT_GMAG_DEADZONE 6
#endif

/* Render tile limits (single-blit path). */
#define EDGEAI_TILE_MAX_W 200
#define EDGEAI_TILE_MAX_H 200

/* Phase 1 post-process: CPU fixed-weight glow (placeholder for NPU). */
#ifndef EDGEAI_ENABLE_POST_GLOW
#define EDGEAI_ENABLE_POST_GLOW 1
#endif
