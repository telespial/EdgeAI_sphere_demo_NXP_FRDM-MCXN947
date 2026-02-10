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

/* Third-dimension ("Z") control.
 * Z is represented as a latched ball scale factor, not an absolute height.
 *
 * Rationale: an accelerometer cannot measure absolute height; it only measures acceleration.
 * The firmware interprets up/down motion as gestures that adjust a persistent scale state.
 *
 * Scale is Q16:
 * - 1.0 == (1<<16)
 * - 0.333.. == (1<<16)/3
 * - 3.0 == (3<<16)
 */
#ifndef EDGEAI_BALL_Z_SCALE_MIN_Q16
#define EDGEAI_BALL_Z_SCALE_MIN_Q16 ((1 << 16) / 3) /* ~0.333x */
#endif
#ifndef EDGEAI_BALL_Z_SCALE_MAX_Q16
#define EDGEAI_BALL_Z_SCALE_MAX_Q16 (3 << 16) /* 3.0x */
#endif

/* Convenience radii for bounds/padding when scale is enabled. */
#define EDGEAI_BALL_R_MAX_DRAW ((EDGEAI_BALL_R_MAX * EDGEAI_BALL_Z_SCALE_MAX_Q16) >> 16)
#define EDGEAI_BALL_R_MIN_DRAW ((EDGEAI_BALL_R_MIN * EDGEAI_BALL_Z_SCALE_MIN_Q16) >> 16)

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

/* Z scale smoothing (simulation). */
#ifndef EDGEAI_BALL_Z_SCALE_SMOOTH_SHIFT
#define EDGEAI_BALL_Z_SCALE_SMOOTH_SHIFT 3
#endif

/* Z gesture detection uses a high-pass signal from accel magnitude |a| (raw counts). */
#ifndef EDGEAI_BALL_Z_GMAG_LP_SHIFT
#define EDGEAI_BALL_Z_GMAG_LP_SHIFT 6
#endif
#ifndef EDGEAI_BALL_Z_GMAG_RANGE
#define EDGEAI_BALL_Z_GMAG_RANGE 80
#endif
#ifndef EDGEAI_BALL_Z_GMAG_DEADZONE
#define EDGEAI_BALL_Z_GMAG_DEADZONE 6
#endif

/* Z latch tuning: threshold + per-second rate at full magnitude. */
#ifndef EDGEAI_BALL_Z_LATCH_THR
#define EDGEAI_BALL_Z_LATCH_THR 10
#endif
#ifndef EDGEAI_BALL_Z_LATCH_LOCKOUT_US
#define EDGEAI_BALL_Z_LATCH_LOCKOUT_US 180000u
#endif
#ifndef EDGEAI_BALL_Z_LATCH_RATE_Q16_PER_S
#define EDGEAI_BALL_Z_LATCH_RATE_Q16_PER_S (6 << 16)
#endif

/* Optional visual lift derived from Z scale. This keeps a depth cue even when Z is
 * represented primarily by size.
 */
#ifndef EDGEAI_BALL_Z_LIFT_MAX_PX
#define EDGEAI_BALL_Z_LIFT_MAX_PX 28
#endif

static inline int32_t edgeai_ball_lift_px_for_scale_q16(int32_t scale_q16)
{
    const int32_t one_q16 = (1 << 16);
    if (EDGEAI_BALL_Z_LIFT_MAX_PX <= 0) return 0;

    int32_t lift = 0;
    if (scale_q16 >= one_q16)
    {
        int32_t den = EDGEAI_BALL_Z_SCALE_MAX_Q16 - one_q16;
        if (den <= 0) return 0;
        lift = (int32_t)(((int64_t)(scale_q16 - one_q16) * EDGEAI_BALL_Z_LIFT_MAX_PX) / den);
    }
    else
    {
        int32_t den = one_q16 - EDGEAI_BALL_Z_SCALE_MIN_Q16;
        if (den <= 0) return 0;
        lift = -(int32_t)(((int64_t)(one_q16 - scale_q16) * EDGEAI_BALL_Z_LIFT_MAX_PX) / den);
    }

    if (lift > EDGEAI_BALL_Z_LIFT_MAX_PX) lift = EDGEAI_BALL_Z_LIFT_MAX_PX;
    if (lift < -EDGEAI_BALL_Z_LIFT_MAX_PX) lift = -EDGEAI_BALL_Z_LIFT_MAX_PX;
    return lift;
}

/* Render tile limits (single-blit path). */
#define EDGEAI_TILE_MAX_W 200
#define EDGEAI_TILE_MAX_H 200
