#include "sim_world.h"

#include "edgeai_config.h"
#include "edgeai_util.h"

void sim_world_init(sim_world_t *w, int32_t lcd_w, int32_t lcd_h)
{
    if (!w) return;
    w->ball.x_q16 = (lcd_w / 2) << 16;
    w->ball.y_q16 = (lcd_h / 2) << 16;
    w->ball.vx_q16 = 0;
    w->ball.vy_q16 = 0;
    w->ball.z_scale_q16 = (1 << 16);
    w->ball.glint = 0;
}

void sim_step(sim_world_t *w, const sim_input_t *in, const sim_params_t *p)
{
    if (!w || !in || !p) return;

    w->ball.z_scale_q16 += (in->z_scale_target_q16 - w->ball.z_scale_q16) >> EDGEAI_BALL_Z_SCALE_SMOOTH_SHIFT;
    w->ball.z_scale_q16 = edgeai_clamp_i32(w->ball.z_scale_q16,
                                           EDGEAI_BALL_Z_SCALE_MIN_Q16,
                                           EDGEAI_BALL_Z_SCALE_MAX_Q16);

    /* One-shot velocity impulse from an impact/bang (provided by the main loop). */
    w->ball.vx_q16 += in->bang_dvx_q16;
    w->ball.vy_q16 += in->bang_dvy_q16;

    /* soft_q15 * a_px_s2 gives Q15 px/s^2; convert to Q16 by <<1 */
    int32_t ax_a_q16 = (int32_t)(((int64_t)in->ax_soft_q15 * p->a_px_s2) << 1);
    int32_t ay_a_q16 = (int32_t)(((int64_t)in->ay_soft_q15 * p->a_px_s2) << 1);

    w->ball.vx_q16 += (int32_t)(((int64_t)ax_a_q16 * p->sim_step_q16) >> 16);
    w->ball.vy_q16 += (int32_t)(((int64_t)ay_a_q16 * p->sim_step_q16) >> 16);

    w->ball.vx_q16 = (int32_t)(((int64_t)w->ball.vx_q16 * p->damp_q16) >> 16);
    w->ball.vy_q16 = (int32_t)(((int64_t)w->ball.vy_q16 * p->damp_q16) >> 16);

    w->ball.x_q16 += (int32_t)(((int64_t)w->ball.vx_q16 * p->sim_step_q16) >> 16);
    w->ball.y_q16 += (int32_t)(((int64_t)w->ball.vy_q16 * p->sim_step_q16) >> 16);

    int32_t cx = w->ball.x_q16 >> 16;
    int32_t cy = w->ball.y_q16 >> 16;

    /* Bounds are based on the ball's maximum radius, but the ball is rendered with a
     * perspective-sized radius. Expand/shrink bounds per step so the collision radius
     * matches what is drawn.
     */
    int32_t r_base = edgeai_ball_r_for_y(cy);
    int32_t r_phys = (int32_t)(((int64_t)r_base * w->ball.z_scale_q16) >> 16);
    if (r_phys < 1) r_phys = 1;
    int32_t shrink = EDGEAI_BALL_R_MAX_DRAW - r_phys;
    int32_t minx = p->minx - shrink;
    int32_t maxx = p->maxx + shrink;
    int32_t miny = p->miny - shrink;
    int32_t maxy = p->maxy + shrink;

    if (cx < minx) { cx = minx; w->ball.x_q16 = cx << 16; w->ball.vx_q16 = -(w->ball.vx_q16 * 3) / 4; }
    if (cx > maxx) { cx = maxx; w->ball.x_q16 = cx << 16; w->ball.vx_q16 = -(w->ball.vx_q16 * 3) / 4; }
    if (cy < miny) { cy = miny; w->ball.y_q16 = cy << 16; w->ball.vy_q16 = -(w->ball.vy_q16 * 3) / 4; }
    if (cy > maxy) { cy = maxy; w->ball.y_q16 = cy << 16; w->ball.vy_q16 = -(w->ball.vy_q16 * 3) / 4; }
}
