#include "render_world.h"

#include "edgeai_config.h"
#include "edgeai_util.h"
#include "par_lcd_s035.h"
#include "sw_render.h"
#include "text5x7.h"

#ifndef EDGEAI_RENDER_SINGLE_BLIT
#define EDGEAI_RENDER_SINGLE_BLIT 1
#endif

/* Sine in Q14 for angles 0..90 degrees in 64 steps. */
static const int16_t s_sin_q14_quarter[65] = {
        0,   402,   804,  1205,  1606,  2006,  2404,  2801,
     3196,  3590,  3981,  4370,  4756,  5139,  5520,  5897,
     6270,  6639,  7005,  7366,  7723,  8076,  8423,  8765,
     9102,  9434,  9760, 10080, 10394, 10702, 11003, 11297,
    11585, 11866, 12140, 12406, 12665, 12916, 13160, 13395,
    13623, 13842, 14053, 14256, 14449, 14635, 14811, 14978,
    15137, 15286, 15426, 15557, 15679, 15791, 15893, 15986,
    16069, 16143, 16207, 16261, 16305, 16340, 16364, 16379,
    16384,
};

static inline void render_sincos_q14(uint8_t a, int32_t *sin_q14, int32_t *cos_q14)
{
    /* a: 0..255 maps to 0..2*pi. */
    uint32_t quad = (uint32_t)(a >> 6); /* 0..3 */
    uint32_t off = (uint32_t)(a & 63u);
    int32_t s = 0;
    int32_t c = 0;
    switch (quad)
    {
        case 0: /* 0..90 */
            s = s_sin_q14_quarter[off];
            c = s_sin_q14_quarter[64u - off];
            break;
        case 1: /* 90..180 */
            s = s_sin_q14_quarter[64u - off];
            c = -s_sin_q14_quarter[off];
            break;
        case 2: /* 180..270 */
            s = -s_sin_q14_quarter[off];
            c = -s_sin_q14_quarter[64u - off];
            break;
        default: /* 270..360 */
            s = -s_sin_q14_quarter[64u - off];
            c = s_sin_q14_quarter[off];
            break;
    }
    *sin_q14 = s;
    *cos_q14 = c;
}

#if EDGEAI_RENDER_SINGLE_BLIT
/* Single tile buffer shared by all renderer paths. */
static uint16_t s_tile[EDGEAI_TILE_MAX_W * EDGEAI_TILE_MAX_H];
#endif

void render_world_init(render_state_t *rs, int32_t cx, int32_t cy)
{
    if (!rs) return;
    rs->trail_head = 0;
    for (int i = 0; i < EDGEAI_TRAIL_N; i++)
    {
        rs->trail_x[i] = (int16_t)cx;
        rs->trail_y[i] = (int16_t)cy;
    }
    rs->prev_x = cx;
    rs->prev_y = cy;
    rs->prev_cy_ground = cy;
    rs->prev_r_draw = edgeai_ball_r_for_y(cy);
    rs->prev_r_shadow = rs->prev_r_draw;
    rs->frame = 0;
}

void render_world_draw_full_background(void)
{
#if EDGEAI_RENDER_SINGLE_BLIT
    for (int32_t y0 = 0; y0 < EDGEAI_LCD_H; y0 += EDGEAI_TILE_MAX_H)
    {
        int32_t y1 = y0 + EDGEAI_TILE_MAX_H - 1;
        if (y1 >= EDGEAI_LCD_H) y1 = EDGEAI_LCD_H - 1;
        int32_t h = y1 - y0 + 1;

        for (int32_t x0 = 0; x0 < EDGEAI_LCD_W; x0 += EDGEAI_TILE_MAX_W)
        {
            int32_t x1 = x0 + EDGEAI_TILE_MAX_W - 1;
            if (x1 >= EDGEAI_LCD_W) x1 = EDGEAI_LCD_W - 1;
            int32_t w = x1 - x0 + 1;

            sw_render_dune_bg(s_tile, (uint32_t)w, (uint32_t)h, x0, y0);
            par_lcd_s035_blit_rect(x0, y0, x1, y1, s_tile);
        }
    }
#endif
}

static void render_world_draw_hud_tile(uint16_t *dst, uint32_t w, uint32_t h,
                                      int32_t x0, int32_t y0,
                                      const render_hud_t *hud)
{
    if (!dst || !hud) return;

    /* HUD region (top-right). */
    const int32_t ov_w = 120;
    const int32_t ov_x0 = EDGEAI_LCD_W - ov_w - 2;
    const int32_t ov_y0 = 2;

    char d3[4];
    edgeai_u32_to_dec3(d3, hud->fps_last);
    char status[18];
    /* Format: C:XYZ B:S N:0 I:0 */
    status[0] = 'C'; status[1] = ':'; status[2] = d3[0]; status[3] = d3[1]; status[4] = d3[2];
    status[5] = ' '; status[6] = 'B'; status[7] = ':'; status[8] = hud->npu_backend;
    status[9] = ' '; status[10] = 'N'; status[11] = ':'; status[12] = hud->npu_init_ok ? '1' : '0';
    status[13] = ' '; status[14] = 'I'; status[15] = ':'; status[16] = hud->npu_run_enabled ? '1' : '0';
    status[17] = '\0';

    sw_render_text5x7(dst, w, h, x0, y0, ov_x0, ov_y0, status, 0x001Fu);
}

static void render_world_union_ball_and_shadow_bounds(int32_t cx, int32_t cy_ground, int32_t cy_draw,
                                                      int32_t r_draw, int32_t r_shadow,
                                                      int32_t *io_x0, int32_t *io_y0,
                                                      int32_t *io_x1, int32_t *io_y1)
{
    if (!io_x0 || !io_y0 || !io_x1 || !io_y1) return;

    int32_t x0 = cx - r_draw;
    int32_t y0 = cy_draw - r_draw;
    int32_t x1 = cx + r_draw;
    int32_t y1 = cy_draw + r_draw;

    /* Shadow params match sw_render_ball_shadow(). */
    int32_t sh_cx = cx + (r_shadow / 4);
    int32_t sh_cy = cy_ground + r_shadow + (r_shadow / 2) + 8;
    int32_t rx = r_shadow + 18;
    int32_t ry = (r_shadow / 2) + 10;

    int32_t sx0 = sh_cx - rx;
    int32_t sy0 = sh_cy - ry;
    int32_t sx1 = sh_cx + rx;
    int32_t sy1 = sh_cy + ry;

    if (sx0 < x0) x0 = sx0;
    if (sy0 < y0) y0 = sy0;
    if (sx1 > x1) x1 = sx1;
    if (sy1 > y1) y1 = sy1;

    if (x0 < *io_x0) *io_x0 = x0;
    if (y0 < *io_y0) *io_y0 = y0;
    if (x1 > *io_x1) *io_x1 = x1;
    if (y1 > *io_y1) *io_y1 = y1;
}

bool render_world_draw(render_state_t *rs,
                       const sim_world_t *world,
                       bool do_render,
                       const render_hud_t *hud)
{
    if (!rs || !world || !hud) return false;
    if (!do_render) return false;

    int32_t cx = world->ball.x_q16 >> 16;
    int32_t cy_ground = world->ball.y_q16 >> 16;
    int32_t z_scale_q16 = world->ball.z_scale_q16;
    z_scale_q16 = edgeai_clamp_i32(z_scale_q16,
                                   EDGEAI_BALL_Z_SCALE_MIN_Q16,
                                   EDGEAI_BALL_Z_SCALE_MAX_Q16);
    int32_t lift_px = edgeai_ball_lift_px_for_scale_q16(z_scale_q16);
    int32_t cy_draw = cy_ground - lift_px;

    int32_t r_base = edgeai_ball_r_for_y(cy_ground);
    int32_t r_draw = (int32_t)(((int64_t)r_base * z_scale_q16) >> 16);
    r_draw = edgeai_clamp_i32(r_draw, 1, EDGEAI_BALL_R_MAX_DRAW);
    int32_t r_shadow = r_draw;

    /* Approximate spin: advance a phase accumulator proportional to speed/r.
     * This makes environment reflections and sparkles "roll" as the ball moves.
     */
    int32_t vx = world->ball.vx_q16 >> 16; /* px/s */
    int32_t vy = world->ball.vy_q16 >> 16; /* px/s */
    int32_t speed = edgeai_abs_i32(vx) + edgeai_abs_i32(vy);
    uint32_t phase_inc = 0;
    if (speed > 0)
    {
        int32_t r_for_spin = (r_draw < 8) ? 8 : r_draw;
        phase_inc = 1u + (uint32_t)(speed / r_for_spin);
    }
    rs->frame += phase_inc;
    uint32_t phase = rs->frame;
    int32_t spin_sin_q14 = 0;
    int32_t spin_cos_q14 = (1 << 14);
    render_sincos_q14((uint8_t)phase, &spin_sin_q14, &spin_cos_q14);

    int32_t shadow_alpha = 60;
    if (EDGEAI_BALL_Z_LIFT_MAX_PX > 0)
    {
        shadow_alpha = 60 - (lift_px * 24) / EDGEAI_BALL_Z_LIFT_MAX_PX;
        shadow_alpha = edgeai_clamp_i32(shadow_alpha, 24, 96);
    }

    int32_t removed_tx = rs->trail_x[rs->trail_head];
    int32_t removed_ty = rs->trail_y[rs->trail_head];

    rs->trail_x[rs->trail_head] = (int16_t)cx;
    rs->trail_y[rs->trail_head] = (int16_t)cy_ground;
    rs->trail_head = (rs->trail_head + 1u) % EDGEAI_TRAIL_N;

    /* Dirty rect is the union of:
     * - current ball + shadow
     * - previous ball + shadow (for clearing)
     * - removed trail point (for clearing)
     * - HUD overlay region
     *
     * This avoids the "clamp to 200x200" artifact that can clip the ball/trails.
     */
    int32_t x0 = cx, y0 = cy_ground, x1 = cx, y1 = cy_ground;
    render_world_union_ball_and_shadow_bounds(cx, cy_ground, cy_draw, r_draw, r_shadow, &x0, &y0, &x1, &y1);
    render_world_union_ball_and_shadow_bounds(rs->prev_x, rs->prev_cy_ground, rs->prev_y,
                                              rs->prev_r_draw, rs->prev_r_shadow,
                                              &x0, &y0, &x1, &y1);

    const int32_t erase_pad = 6; /* dot radius <= 2; keep a small safety margin */
    if ((removed_tx - erase_pad) < x0) x0 = removed_tx - erase_pad;
    if ((removed_ty - erase_pad) < y0) y0 = removed_ty - erase_pad;
    if ((removed_tx + erase_pad) > x1) x1 = removed_tx + erase_pad;
    if ((removed_ty + erase_pad) > y1) y1 = removed_ty + erase_pad;

    /* HUD region (top-right). */
    const int32_t ov_w = 120;
    const int32_t ov_h = 9;
    const int32_t ov_x0 = EDGEAI_LCD_W - ov_w - 2;
    const int32_t ov_y0 = 2;
    const int32_t ov_x1 = EDGEAI_LCD_W - 2;
    const int32_t ov_y1 = ov_y0 + ov_h - 1;
    if (ov_x0 < x0) x0 = ov_x0;
    if (ov_y0 < y0) y0 = ov_y0;
    if (ov_x1 > x1) x1 = ov_x1;
    if (ov_y1 > y1) y1 = ov_y1;

    x0 = edgeai_clamp_i32(x0, 0, EDGEAI_LCD_W - 1);
    y0 = edgeai_clamp_i32(y0, 0, EDGEAI_LCD_H - 1);
    x1 = edgeai_clamp_i32(x1, 0, EDGEAI_LCD_W - 1);
    y1 = edgeai_clamp_i32(y1, 0, EDGEAI_LCD_H - 1);

#if EDGEAI_RENDER_SINGLE_BLIT
    for (int32_t ty0 = y0; ty0 <= y1; ty0 += EDGEAI_TILE_MAX_H)
    {
        int32_t ty1 = ty0 + EDGEAI_TILE_MAX_H - 1;
        if (ty1 > y1) ty1 = y1;
        int32_t th = ty1 - ty0 + 1;

        for (int32_t tx0 = x0; tx0 <= x1; tx0 += EDGEAI_TILE_MAX_W)
        {
            int32_t tx1 = tx0 + EDGEAI_TILE_MAX_W - 1;
            if (tx1 > x1) tx1 = x1;
            int32_t tw = tx1 - tx0 + 1;

            sw_render_dune_bg(s_tile, (uint32_t)tw, (uint32_t)th, tx0, ty0);

            for (int i = 0; i < EDGEAI_TRAIL_N; i++)
            {
                uint32_t idx = (rs->trail_head + (uint32_t)i) % EDGEAI_TRAIL_N;
                int32_t tx = rs->trail_x[idx];
                int32_t ty = rs->trail_y[idx];
                int r0 = 1 + (i / 6);
                uint16_t c = (i < 6) ? 0x39E7u : 0x18C3u;
                sw_render_filled_circle(s_tile, (uint32_t)tw, (uint32_t)th, tx0, ty0, tx, ty, r0, c);
            }

            sw_render_ball_shadow(s_tile, (uint32_t)tw, (uint32_t)th, tx0, ty0,
                                  cx, cy_ground, r_shadow, (uint32_t)shadow_alpha);
            sw_render_silver_ball(s_tile, (uint32_t)tw, (uint32_t)th, tx0, ty0,
                                  cx, cy_draw, r_draw, phase, world->ball.glint, spin_sin_q14, spin_cos_q14);
            render_world_draw_hud_tile(s_tile, (uint32_t)tw, (uint32_t)th, tx0, ty0, hud);

            par_lcd_s035_blit_rect(tx0, ty0, tx1, ty1, s_tile);
        }
    }
#else
    uint16_t bg = hud->accel_fail ? 0x1800u : 0x0000u;
    par_lcd_s035_fill_rect(x0, y0, x1, y1, bg);

    for (int i = 0; i < EDGEAI_TRAIL_N; i++)
    {
        uint32_t idx = (rs->trail_head + (uint32_t)i) % EDGEAI_TRAIL_N;
        int32_t tx = rs->trail_x[idx];
        int32_t ty = rs->trail_y[idx];
        int r0 = 1 + (i / 6);
        uint16_t c = (i < 6) ? 0x39E7u : 0x18C3u;
        par_lcd_s035_draw_filled_circle(tx, ty, r0, c);
    }

	    par_lcd_s035_draw_ball_shadow(cx, cy_ground, r_shadow, (uint32_t)shadow_alpha);
	    par_lcd_s035_draw_silver_ball(cx, cy_draw, r_draw, phase, world->ball.glint, spin_sin_q14, spin_cos_q14);

    char d3[4];
    edgeai_u32_to_dec3(d3, hud->fps_last);
    char status[40];
    status[0] = 'C'; status[1] = ':'; status[2] = d3[0]; status[3] = d3[1]; status[4] = d3[2];
    status[5] = ' '; status[6] = 'B'; status[7] = ':'; status[8] = hud->npu_backend;
    status[9] = ' '; status[10] = 'N'; status[11] = ':'; status[12] = hud->npu_init_ok ? '1' : '0';
    status[13] = ' '; status[14] = 'I'; status[15] = ':'; status[16] = hud->npu_run_enabled ? '1' : '0';
    status[17] = '\0';
    par_lcd_s035_fill_rect(ov_x0, ov_y0, ov_x1, ov_y1, 0x0000u);
    edgeai_text5x7_draw_scaled(ov_x0, ov_y0, 1, status, 0x001Fu);
#endif

    rs->prev_x = cx;
    rs->prev_y = cy_draw;
    rs->prev_cy_ground = cy_ground;
    rs->prev_r_draw = r_draw;
    rs->prev_r_shadow = r_shadow;
    return true;
}
