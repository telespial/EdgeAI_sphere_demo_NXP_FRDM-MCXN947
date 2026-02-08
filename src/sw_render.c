#include "sw_render.h"

#include <string.h>

#include "dune_bg.h"

static inline uint32_t sw_isqrt_u32(uint32_t x)
{
    /* Integer sqrt (floor). */
    uint32_t op = x;
    uint32_t res = 0;
    uint32_t one = 1uL << 30;
    while (one > op) one >>= 2;
    while (one != 0)
    {
        if (op >= res + one)
        {
            op -= res + one;
            res = res + 2u * one;
        }
        res >>= 1;
        one >>= 2;
    }
    return res;
}

static inline void sw_put(uint16_t *dst, uint32_t w, uint32_t h,
                          int32_t x, int32_t y, uint16_t c)
{
    if ((uint32_t)x >= w || (uint32_t)y >= h) return;
    dst[(uint32_t)y * w + (uint32_t)x] = c;
}

void sw_render_clear(uint16_t *dst, uint32_t w, uint32_t h, uint16_t rgb565)
{
    if (!dst || w == 0 || h == 0) return;
    if (rgb565 == 0)
    {
        memset(dst, 0, (size_t)w * (size_t)h * sizeof(dst[0]));
        return;
    }
    for (uint32_t i = 0; i < w * h; i++) dst[i] = rgb565;
}

void sw_render_dune_bg(uint16_t *dst, uint32_t w, uint32_t h,
                       int32_t x0, int32_t y0)
{
    if (!dst || w == 0 || h == 0) return;

    for (uint32_t y = 0; y < h; y++)
    {
        int32_t gy = y0 + (int32_t)y;
        if (gy < 0) gy = 0;
        uint32_t ty = ((uint32_t)gy) >> 1;
        if (ty >= DUNE_TEX_H) ty = DUNE_TEX_H - 1u;

        const uint16_t *src = &g_dune_tex[ty * DUNE_TEX_W];
        uint16_t *row = &dst[y * w];

        for (uint32_t x = 0; x < w; x++)
        {
            int32_t gx = x0 + (int32_t)x;
            if (gx < 0) gx = 0;
            uint32_t tx = ((uint32_t)gx) >> 1;
            if (tx >= DUNE_TEX_W) tx = DUNE_TEX_W - 1u;
            row[x] = src[tx];
        }
    }
}

void sw_render_dune_bg_line(uint16_t *dst, uint32_t w, int32_t x0, int32_t y)
{
    /* Convenience wrapper for single-scanline blits (boot-time full draw). */
    sw_render_dune_bg(dst, w, 1u, x0, y);
}

void sw_render_filled_circle(uint16_t *dst, uint32_t w, uint32_t h,
                             int32_t x0, int32_t y0,
                             int32_t cx, int32_t cy, int32_t r, uint16_t rgb565)
{
    if (!dst || r <= 0) return;

    /* Convert to buffer-local coords. */
    int32_t lc_x = cx - x0;
    int32_t lc_y = cy - y0;

    int32_t y_min = lc_y - r;
    int32_t y_max = lc_y + r;
    if (y_min < 0) y_min = 0;
    if (y_max >= (int32_t)h) y_max = (int32_t)h - 1;

    const int32_t r2 = r * r;
    for (int32_t y = y_min; y <= y_max; y++)
    {
        int32_t dy = y - lc_y;
        int32_t dy2 = dy * dy;
        if (dy2 > r2) continue;
        int32_t dx_max = (int32_t)sw_isqrt_u32((uint32_t)(r2 - dy2));
        int32_t x_min = lc_x - dx_max;
        int32_t x_max = lc_x + dx_max;
        if (x_min < 0) x_min = 0;
        if (x_max >= (int32_t)w) x_max = (int32_t)w - 1;
        uint16_t *row = &dst[(uint32_t)y * w];
        for (int32_t x = x_min; x <= x_max; x++) row[(uint32_t)x] = rgb565;
    }
}

void sw_render_ball_shadow(uint16_t *dst, uint32_t w, uint32_t h,
                           int32_t x0, int32_t y0,
                           int32_t cx, int32_t cy, int32_t r)
{
    if (!dst || r <= 0) return;

    /* Match the previous "AO spot" look, but render into the tile buffer. */
    int32_t sh_cx = cx + (r / 4);
    int32_t sh_cy = cy + r + (r / 2) + 8;
    int32_t rx = r + 18;
    int32_t ry = (r / 2) + 10;

    int32_t by0 = sh_cy - ry;
    int32_t by1 = sh_cy + ry;

    for (int32_t y = by0; y <= by1; y++)
    {
        int32_t ly = y - y0;
        if ((uint32_t)ly >= h) continue;
        int32_t dy = y - sh_cy;
        uint32_t dy2 = (uint32_t)(dy * dy);

        int32_t bx0 = sh_cx - rx;
        int32_t bx1 = sh_cx + rx;
        for (int32_t x = bx0; x <= bx1; x++)
        {
            int32_t lx = x - x0;
            if ((uint32_t)lx >= w) continue;

            int32_t dx = x - sh_cx;
            uint32_t dx2 = (uint32_t)(dx * dx);

            uint32_t d = (dx2 * 256u) / (uint32_t)(rx * rx) + (dy2 * 256u) / (uint32_t)(ry * ry);
            if (d >= 256u) continue;

            uint32_t t = 256u - d;            /* 0..255 */
            uint32_t a = (t * 60u) / 255u;    /* 0..60 */
            uint16_t c = sw_pack_rgb565_u8(a, a, a + (a / 3));

            /* Max-blend so shadow doesn't erase trails/ball. */
            uint16_t *p = &dst[(uint32_t)ly * w + (uint32_t)lx];
            if (c > *p) *p = c;
        }
    }
}

void sw_render_silver_ball(uint16_t *dst, uint32_t w, uint32_t h,
                           int32_t x0, int32_t y0,
                           int32_t cx, int32_t cy, int32_t r, uint32_t frame, uint8_t glint)
{
    (void)frame;
    if (!dst || r <= 0) return;

    int32_t sx0 = cx - r;
    int32_t sx1 = cx + r;
    int32_t sy0 = cy - r;
    int32_t sy1 = cy + r;

    /* Light direction (normalized-ish) in Q14. */
    const int32_t Lx = -6553;  /* -0.4 */
    const int32_t Ly = -9830;  /* -0.6 */
    const int32_t Lz = 11469;  /*  0.7 */

    const uint32_t r2 = (uint32_t)(r * r);

    for (int32_t y = sy0; y <= sy1; y++)
    {
        int32_t ly = y - y0;
        if ((uint32_t)ly >= h) continue;

        int32_t dy = y - cy;
        int32_t dy2 = dy * dy;
        if ((uint32_t)dy2 > r2) continue;

        uint32_t dx_max_u = sw_isqrt_u32(r2 - (uint32_t)dy2);
        int32_t rx0 = cx - (int32_t)dx_max_u;
        int32_t rx1 = cx + (int32_t)dx_max_u;
        if (rx0 < sx0) rx0 = sx0;
        if (rx1 > sx1) rx1 = sx1;

        for (int32_t x = rx0; x <= rx1; x++)
        {
            int32_t lx = x - x0;
            if ((uint32_t)lx >= w) continue;

            int32_t dx = x - cx;
            uint32_t d2 = (uint32_t)(dx * dx + dy2);
            if (d2 > r2) continue;

            uint32_t zz = sw_isqrt_u32(r2 - d2);

            /* Normal in Q14: n = (dx,dy,z)/r. */
            int32_t nx = (dx << 14) / r;
            int32_t ny = (dy << 14) / r;
            int32_t nz = ((int32_t)zz << 14) / r;

            int32_t ndl = (nx * Lx + ny * Ly + nz * Lz) >> 14;
            if (ndl < 0) ndl = 0;

            int32_t rz = ((2 * ndl * nz) >> 14) - Lz;
            if (rz < 0) rz = 0;

            int32_t s = rz;
            int32_t s2 = (s * s) >> 14;
            int32_t s4 = (s2 * s2) >> 14;
            int32_t s8 = (s4 * s4) >> 14;
            int32_t s16 = (s8 * s8) >> 14;

            int32_t inv = (1 << 14) - nz;
            if (inv < 0) inv = 0;
            int32_t f2 = (inv * inv) >> 14;
            int32_t f4 = (f2 * f2) >> 14;
            int32_t f5 = (f4 * inv) >> 14;

            uint32_t br = 180, bg = 185, bb = 195;

            const int32_t amb = 2949;     /* 0.18 * 16384 */
            const int32_t diff_k = 11469; /* 0.70 * 16384 */
            int32_t spec_k = 20480;       /* 1.25 * 16384 */
            const int32_t fre_k  = 5734;  /* 0.35 * 16384 */
            spec_k += (int32_t)((uint32_t)glint * 8192u / 255u);

            int32_t diff = (ndl * diff_k) >> 14;
            int32_t spec = (s16 * spec_k) >> 14;
            int32_t fre  = (f5  * fre_k) >> 14;
            int32_t I = amb + diff;
            if (I > (3 << 14)) I = (3 << 14);

            uint32_t r8 = (br * (uint32_t)I) >> 14;
            uint32_t g8 = (bg * (uint32_t)I) >> 14;
            uint32_t b8 = (bb * (uint32_t)I) >> 14;

            r8 += (uint32_t)((255u * (uint32_t)spec) >> 14);
            g8 += (uint32_t)((255u * (uint32_t)spec) >> 14);
            b8 += (uint32_t)((255u * (uint32_t)spec) >> 14);

            r8 += (uint32_t)((40u * (uint32_t)fre) >> 14);
            g8 += (uint32_t)((60u * (uint32_t)fre) >> 14);
            b8 += (uint32_t)((90u * (uint32_t)fre) >> 14);

            if (glint > 160u)
            {
                int32_t band = (dx + dy + (r / 3));
                if (band > -2 && band < 2)
                {
                    r8 += 40u;
                    g8 += 50u;
                    b8 += 60u;
                }
            }

            sw_put(dst, w, h, lx, ly, sw_pack_rgb565_u8(r8, g8, b8));
        }
    }
}
