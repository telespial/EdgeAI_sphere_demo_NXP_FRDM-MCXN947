#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sim.h"

/* NXP PAR-LCD-S035 (ST7796S, 480x320, 8080 via FlexIO) bring-up + simple rendering. */

bool par_lcd_s035_init(void);

/* Fill the whole screen with a solid RGB565 color. */
void par_lcd_s035_fill(uint16_t rgb565);

/* Render the sim grid scaled to 480x320 (nearest-neighbor), RGB565.
 *
 * trail: optional buffer at grid resolution, same layout as grid->cells, where 0 means none and
 *        larger values brighten pixels to show motion trails.
 * frame: monotonically increasing counter for animations/shimmer/dither.
 */
void par_lcd_s035_render_grid(const sim_grid_t *grid, const uint8_t *trail, uint32_t frame);
