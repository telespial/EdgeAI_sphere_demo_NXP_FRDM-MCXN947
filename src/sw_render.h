#pragma once

#include <stdint.h>

/* Simple software renderer into a caller-provided RGB565 buffer (row-major).
 * All coordinates are in LCD pixel space; (x0,y0) is the buffer's top-left origin.
 */

static inline uint16_t sw_pack_rgb565_u8(uint32_t r8, uint32_t g8, uint32_t b8)
{
    if (r8 > 255u) r8 = 255u;
    if (g8 > 255u) g8 = 255u;
    if (b8 > 255u) b8 = 255u;
    uint16_t r = (uint16_t)(r8 >> 3);
    uint16_t g = (uint16_t)(g8 >> 2);
    uint16_t b = (uint16_t)(b8 >> 3);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

void sw_render_clear(uint16_t *dst, uint32_t w, uint32_t h, uint16_t rgb565);
void sw_render_dune_bg(uint16_t *dst, uint32_t w, uint32_t h,
                       int32_t x0, int32_t y0);
void sw_render_text5x7(uint16_t *dst, uint32_t w, uint32_t h,
                       int32_t x0, int32_t y0,
                       int32_t x, int32_t y, const char *s, uint16_t rgb565);
void sw_render_filled_circle(uint16_t *dst, uint32_t w, uint32_t h,
                             int32_t x0, int32_t y0,
                             int32_t cx, int32_t cy, int32_t r, uint16_t rgb565);
void sw_render_ball_shadow(uint16_t *dst, uint32_t w, uint32_t h,
                           int32_t x0, int32_t y0,
                           int32_t cx, int32_t cy, int32_t r, uint32_t alpha_max);
void sw_render_silver_ball(uint16_t *dst, uint32_t w, uint32_t h,
                           int32_t x0, int32_t y0,
                           int32_t cx, int32_t cy, int32_t r,
                           uint32_t phase, uint8_t glint,
                           int32_t spin_sin_q14, int32_t spin_cos_q14);
