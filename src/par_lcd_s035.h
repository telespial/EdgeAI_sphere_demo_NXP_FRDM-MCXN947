#pragma once

#include <stdbool.h>
#include <stdint.h>

/* LCD driver notes:
 * - Intentionally simple single-buffer blit API used by this demo.
 * - Future work (DMA, double-buffering, partial redraw scheduling) should live in this module.
 */

bool par_lcd_s035_init(void);
void par_lcd_s035_fill(uint16_t rgb565);

/* Blit a full RGB565 rectangle to the LCD in one transfer (inclusive coords). */
void par_lcd_s035_blit_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t *rgb565);

/* Filled circle in LCD pixel coordinates. */
void par_lcd_s035_draw_filled_circle(int32_t cx, int32_t cy, int32_t r, uint16_t rgb565);

/* Fill rectangle (inclusive) in LCD pixel coordinates. */
void par_lcd_s035_fill_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t rgb565);

/* Draw a shaded "silver" ball. The background should already be set (e.g. black). */
void par_lcd_s035_draw_silver_ball(int32_t cx, int32_t cy, int32_t r,
                                   uint32_t phase, uint8_t glint,
                                   int32_t spin_sin_q14, int32_t spin_cos_q14);

/* Draw a soft ambient-occlusion style shadow below the ball (visible even on black). */
void par_lcd_s035_draw_ball_shadow(int32_t cx, int32_t cy, int32_t r, uint32_t alpha_max);
