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

bool render_world_draw(render_state_t *rs,
                       const sim_world_t *world,
                       bool do_render,
                       const render_hud_t *hud)
{
    if (!rs || !world || !hud) return false;
    if (!do_render) return false;

    int32_t cx = world->ball.x_q16 >> 16;
    int32_t cy_ground = world->ball.y_q16 >> 16;
    int32_t lift_px = world->ball.lift_q16 >> 16;
    lift_px = edgeai_clamp_i32_sym(lift_px, EDGEAI_BALL_LIFT_MAX_PX);
    int32_t cy_draw = cy_ground - lift_px;

    int32_t r_ground = edgeai_ball_r_for_y(cy_ground);
    int32_t r_draw = r_ground + (lift_px / 4);
    r_draw = edgeai_clamp_i32(r_draw, EDGEAI_BALL_R_MIN, EDGEAI_BALL_R_MAX);

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
    if (EDGEAI_BALL_LIFT_MAX_PX > 0)
    {
        shadow_alpha = 60 - (lift_px * 24) / EDGEAI_BALL_LIFT_MAX_PX;
        shadow_alpha = edgeai_clamp_i32(shadow_alpha, 24, 96);
    }

    int32_t removed_tx = rs->trail_x[rs->trail_head];
    int32_t removed_ty = rs->trail_y[rs->trail_head];

    rs->trail_x[rs->trail_head] = (int16_t)cx;
    rs->trail_y[rs->trail_head] = (int16_t)cy_ground;
    rs->trail_head = (rs->trail_head + 1u) % EDGEAI_TRAIL_N;

    int32_t minx_r = cx, miny_r = cy_ground, maxx_r = cx, maxy_r = cy_ground;
    if (cy_draw < miny_r) miny_r = cy_draw;
    if (cy_draw > maxy_r) maxy_r = cy_draw;
    for (int i = 0; i < EDGEAI_TRAIL_N; i++)
    {
        int32_t tx = rs->trail_x[i];
        int32_t ty = rs->trail_y[i];
        if (tx < minx_r) minx_r = tx;
        if (ty < miny_r) miny_r = ty;
        if (tx > maxx_r) maxx_r = tx;
        if (ty > maxy_r) maxy_r = ty;
    }
    if (removed_tx < minx_r) minx_r = removed_tx;
    if (removed_ty < miny_r) miny_r = removed_ty;
    if (removed_tx > maxx_r) maxx_r = removed_tx;
    if (removed_ty > maxy_r) maxy_r = removed_ty;
    if (rs->prev_x < minx_r) minx_r = rs->prev_x;
    if (rs->prev_y < miny_r) miny_r = rs->prev_y;
    if (rs->prev_x > maxx_r) maxx_r = rs->prev_x;
    if (rs->prev_y > maxy_r) maxy_r = rs->prev_y;

    int32_t pad = EDGEAI_BALL_R_MAX + 30;
    int32_t x0 = edgeai_clamp_i32(minx_r - pad, 0, EDGEAI_LCD_W - 1);
    int32_t y0 = edgeai_clamp_i32(miny_r - pad, 0, EDGEAI_LCD_H - 1);
    int32_t x1 = edgeai_clamp_i32(maxx_r + pad, 0, EDGEAI_LCD_W - 1);
    int32_t y1 = edgeai_clamp_i32(maxy_r + pad, 0, EDGEAI_LCD_H - 1);

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

    int32_t w = x1 - x0 + 1;
    int32_t h = y1 - y0 + 1;
    bool did_clamp = false;
    if (w > EDGEAI_TILE_MAX_W || h > EDGEAI_TILE_MAX_H)
    {
        did_clamp = true;
        int32_t halfw = EDGEAI_TILE_MAX_W / 2;
        int32_t halfh = EDGEAI_TILE_MAX_H / 2;
        int32_t ccx = (minx_r + maxx_r) / 2;
        int32_t ccy = (miny_r + maxy_r) / 2;
        x0 = edgeai_clamp_i32(ccx - halfw, 0, EDGEAI_LCD_W - 1);
        y0 = edgeai_clamp_i32(ccy - halfh, 0, EDGEAI_LCD_H - 1);
        x1 = edgeai_clamp_i32(x0 + EDGEAI_TILE_MAX_W - 1, 0, EDGEAI_LCD_W - 1);
        y1 = edgeai_clamp_i32(y0 + EDGEAI_TILE_MAX_H - 1, 0, EDGEAI_LCD_H - 1);
        w = x1 - x0 + 1;
        h = y1 - y0 + 1;
    }

#if EDGEAI_RENDER_SINGLE_BLIT
    sw_render_dune_bg(s_tile, (uint32_t)w, (uint32_t)h, x0, y0);

    for (int i = 0; i < EDGEAI_TRAIL_N; i++)
    {
        uint32_t idx = (rs->trail_head + (uint32_t)i) % EDGEAI_TRAIL_N;
        int32_t tx = rs->trail_x[idx];
        int32_t ty = rs->trail_y[idx];
        int r0 = 1 + (i / 6);
        uint16_t c = (i < 6) ? 0x39E7u : 0x18C3u;
        sw_render_filled_circle(s_tile, (uint32_t)w, (uint32_t)h, x0, y0, tx, ty, r0, c);
    }

    sw_render_ball_shadow(s_tile, (uint32_t)w, (uint32_t)h, x0, y0, cx, cy_ground, r_ground, (uint32_t)shadow_alpha);
    sw_render_silver_ball(s_tile, (uint32_t)w, (uint32_t)h, x0, y0,
                          cx, cy_draw, r_draw, phase, world->ball.glint, spin_sin_q14, spin_cos_q14);
		    render_world_draw_hud_tile(s_tile, (uint32_t)w, (uint32_t)h, x0, y0, hud);

		    par_lcd_s035_blit_rect(x0, y0, x1, y1, s_tile);

	    /* If the main dirty-rect is clamped, the removed trail point can fall outside the
	     * final blit region and remain "stuck" on the LCD. Issue a tiny cleanup blit around
	     * the removed point to clear stale pixels.
	     */
	    if (did_clamp)
	    {
	        const int32_t erase_pad = 6; /* dot radius <= 2; keep a small safety margin */
	        if ((removed_tx - erase_pad) < x0 || (removed_tx + erase_pad) > x1 ||
	            (removed_ty - erase_pad) < y0 || (removed_ty + erase_pad) > y1)
	        {
	            int32_t ex0 = edgeai_clamp_i32(removed_tx - erase_pad, 0, EDGEAI_LCD_W - 1);
	            int32_t ey0 = edgeai_clamp_i32(removed_ty - erase_pad, 0, EDGEAI_LCD_H - 1);
	            int32_t ex1 = edgeai_clamp_i32(removed_tx + erase_pad, 0, EDGEAI_LCD_W - 1);
	            int32_t ey1 = edgeai_clamp_i32(removed_ty + erase_pad, 0, EDGEAI_LCD_H - 1);
	            int32_t ew = ex1 - ex0 + 1;
	            int32_t eh = ey1 - ey0 + 1;

	            sw_render_dune_bg(s_tile, (uint32_t)ew, (uint32_t)eh, ex0, ey0);

	            for (int i = 0; i < EDGEAI_TRAIL_N; i++)
	            {
                uint32_t idx = (rs->trail_head + (uint32_t)i) % EDGEAI_TRAIL_N;
                int32_t tx = rs->trail_x[idx];
                int32_t ty = rs->trail_y[idx];
                int r0 = 1 + (i / 6);
                uint16_t c = (i < 6) ? 0x39E7u : 0x18C3u;
                sw_render_filled_circle(s_tile, (uint32_t)ew, (uint32_t)eh, ex0, ey0, tx, ty, r0, c);
            }

            sw_render_ball_shadow(s_tile, (uint32_t)ew, (uint32_t)eh, ex0, ey0, cx, cy_ground, r_ground, (uint32_t)shadow_alpha);
            sw_render_silver_ball(s_tile, (uint32_t)ew, (uint32_t)eh, ex0, ey0,
                                  cx, cy_draw, r_draw, phase, world->ball.glint, spin_sin_q14, spin_cos_q14);
            render_world_draw_hud_tile(s_tile, (uint32_t)ew, (uint32_t)eh, ex0, ey0, hud);

		            par_lcd_s035_blit_rect(ex0, ey0, ex1, ey1, s_tile);
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

	    par_lcd_s035_draw_ball_shadow(cx, cy_ground, r_ground, (uint32_t)shadow_alpha);
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
    return true;
}
