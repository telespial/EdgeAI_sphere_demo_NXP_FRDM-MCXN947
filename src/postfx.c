#include "postfx.h"

#include "npu_postfx.h"

/* Subtle scanlines + vignette for a more "finished" look.
 * This is intentionally cheap and deterministic.
 */
void postfx_apply_line_rgb565(uint16_t *line, uint32_t width, uint32_t y, uint32_t frame)
{
    if (!line || width == 0)
    {
        return;
    }

#if defined(CONFIG_EDGEAI_USE_NPU_POSTFX) && (CONFIG_EDGEAI_USE_NPU_POSTFX == 1)
    if (npu_postfx_apply_line_rgb565(line, width, y, frame))
    {
        return;
    }
#endif

    /* Scanlines: darken every other row slightly. */
    const int scan = (y & 1u) ? -1 : 0;

    for (uint32_t x = 0; x < width; x++)
    {
        uint16_t c = line[x];
        int r = (c >> 11) & 0x1F;
        int g = (c >> 5) & 0x3F;
        int b = (c >> 0) & 0x1F;

        /* Vignette: darken towards the edges. */
        uint32_t edge = x;
        if (edge > (width - 1u - x)) edge = (width - 1u - x);
        int vig = 0;
        if (edge < 12u) vig = -2;
        else if (edge < 24u) vig = -1;

        r += scan + vig;
        g += (scan * 2) + (vig * 2);
        b += scan + vig;

        if (r < 0) { r = 0; }
        if (r > 31) { r = 31; }
        if (g < 0) { g = 0; }
        if (g > 63) { g = 63; }
        if (b < 0) { b = 0; }
        if (b > 31) { b = 31; }

        line[x] = (uint16_t)((r << 11) | (g << 5) | (b << 0));
    }
}
