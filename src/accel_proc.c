#include "accel_proc.h"

#include "edgeai_config.h"
#include "edgeai_util.h"

void accel_proc_init(accel_proc_t *s)
{
    if (!s) return;
    s->ax_lp = 0;
    s->ay_lp = 0;
    s->az_lp = 0;
    s->bang_prev = false;
}

void accel_proc_apply_axis_map(int32_t *ax, int32_t *ay)
{
    if (!ax || !ay) return;
#if EDGEAI_ACCEL_SWAP_XY
    int32_t tmp = *ax; *ax = *ay; *ay = tmp;
#endif
#if EDGEAI_ACCEL_INVERT_X
    *ax = -*ax;
#endif
#if EDGEAI_ACCEL_INVERT_Y
    *ay = -*ay;
#endif
}

void accel_proc_update(accel_proc_t *s, int32_t raw_x, int32_t raw_y, int32_t raw_z, accel_proc_out_t *out)
{
    if (!s || !out) return;

    int32_t ax = raw_x;
    int32_t ay = raw_y;
    int32_t az = raw_z;
    accel_proc_apply_axis_map(&ax, &ay);

    /* Filter intent (tilt path):
     * - LP (`>>2`) smooths sensor noise while keeping tilts responsive.
     * - Deadzone removes small jitter near level.
     * - Soft-response blend (linear+cubic) reduces twitch near center while preserving strong tilt authority.
     *
     * Tuning symptoms:
     * - Jitter at rest: increase deadzone or LP strength (larger shift).
     * - Sluggish tilt: decrease LP strength (smaller shift) or deadzone.
     * - Impacts not visible: adjust bang threshold/gain (high-pass path), not the tilt LP.
     */

    /* Fast low-pass so small tilts respond quickly. */
    s->ax_lp += (ax - s->ax_lp) >> 2;
    s->ay_lp += (ay - s->ay_lp) >> 2;
    s->az_lp += (az - s->az_lp) >> 2;

    /* Clamp to limit I2C glitches and avoid sudden large steps. */
    const int32_t clamp_lim = EDGEAI_ACCEL_MAP_DENOM * 2;
    s->ax_lp = edgeai_clamp_i32(s->ax_lp, -clamp_lim, clamp_lim);
    s->ay_lp = edgeai_clamp_i32(s->ay_lp, -clamp_lim, clamp_lim);
    s->az_lp = edgeai_clamp_i32(s->az_lp, -clamp_lim, clamp_lim);

    /* High-pass components for impact detection. */
    int32_t ax_hp = ax - s->ax_lp;
    int32_t ay_hp = ay - s->ay_lp;
    int32_t az_hp = az - s->az_lp;
    int32_t bang_score =
        edgeai_abs_i32(az_hp) +
        (edgeai_abs_i32(ax_hp) >> 1) +
        (edgeai_abs_i32(ay_hp) >> 1);
    bool bang_now = (bang_score > EDGEAI_BANG_THRESHOLD);
    bool bang_pulse = (bang_now && !s->bang_prev);
    s->bang_prev = bang_now;

    /* Deadzoned soft response:
     * - small deadzone reduces jitter
     * - blended linear/cubic curve reduces twitch near center while keeping strong response at larger tilts
     */
    const int32_t deadzone = 10;
    int32_t ax_dz = s->ax_lp;
    int32_t ay_dz = s->ay_lp;
    if (edgeai_abs_i32(ax_dz) <= deadzone) ax_dz = 0;
    else ax_dz -= (ax_dz > 0) ? deadzone : -deadzone;
    if (edgeai_abs_i32(ay_dz) <= deadzone) ay_dz = 0;
    else ay_dz -= (ay_dz > 0) ? deadzone : -deadzone;

    /* Normalize to Q15 ~= [-1,1] at 1g. */
    int32_t ax_n_q15 = (int32_t)(((int64_t)ax_dz << 15) / (int64_t)EDGEAI_ACCEL_MAP_DENOM);
    int32_t ay_n_q15 = (int32_t)(((int64_t)ay_dz << 15) / (int64_t)EDGEAI_ACCEL_MAP_DENOM);
    ax_n_q15 = edgeai_clamp_i32_sym(ax_n_q15, 32767);
    ay_n_q15 = edgeai_clamp_i32_sym(ay_n_q15, 32767);

    /* cubic = x^3 (Q15) */
    int32_t ax2 = (int32_t)(((int64_t)ax_n_q15 * ax_n_q15) >> 15);
    int32_t ay2 = (int32_t)(((int64_t)ay_n_q15 * ay_n_q15) >> 15);
    int32_t ax_cu_q15 = (int32_t)(((int64_t)ax2 * ax_n_q15) >> 15);
    int32_t ay_cu_q15 = (int32_t)(((int64_t)ay2 * ay_n_q15) >> 15);

    /* Blend: 35% linear + 65% cubic (alpha in Q15). */
    const int32_t alpha_q15 = 11469;
    int32_t ax_soft_q15 = (int32_t)(((int64_t)ax_n_q15 * alpha_q15 +
                                     (int64_t)ax_cu_q15 * ((1 << 15) - alpha_q15)) >> 15);
    int32_t ay_soft_q15 = (int32_t)(((int64_t)ay_n_q15 * alpha_q15 +
                                     (int64_t)ay_cu_q15 * ((1 << 15) - alpha_q15)) >> 15);

    out->ax_lp = s->ax_lp;
    out->ay_lp = s->ay_lp;
    out->az_lp = s->az_lp;
    out->ax_hp = ax_hp;
    out->ay_hp = ay_hp;
    out->az_hp = az_hp;
    out->bang_score = bang_score;
    out->ax_soft_q15 = ax_soft_q15;
    out->ay_soft_q15 = ay_soft_q15;
    out->bang_pulse = bang_pulse;
}
