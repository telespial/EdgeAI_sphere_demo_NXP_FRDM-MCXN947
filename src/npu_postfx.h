#pragma once

#include <stdint.h>

/* NPU-backed postfx interface (currently stubbed). */

/* Returns 1 if applied, 0 if not available (caller should fall back). */
int npu_postfx_apply_line_rgb565(uint16_t *line, uint32_t width, uint32_t y, uint32_t frame);

