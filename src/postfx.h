#pragma once

#include <stdint.h>

/* Post-process effects pipeline.
 *
 * Today: CPU-only "looks better" pass (scanlines/vignette).
 * Later: can be swapped for an NPU-backed implementation without changing the renderer.
 */

void postfx_apply_line_rgb565(uint16_t *line, uint32_t width, uint32_t y, uint32_t frame);

