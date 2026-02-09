#include "sw_render.h"

#include <string.h>

#include "dune_bg.h"
#include "edgeai_util.h"

static const uint8_t *sw_glyph5x7(char c)
{
    static const uint8_t SPACE[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t COLON[7] = {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00};

    static const uint8_t B[7] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
    static const uint8_t C[7] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
    static const uint8_t I[7] = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
    static const uint8_t N[7] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
    static const uint8_t S[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};

    static const uint8_t D0[7] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
    static const uint8_t D1[7] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
    static const uint8_t D2[7] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
    static const uint8_t D3[7] = {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E};
    static const uint8_t D4[7] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
    static const uint8_t D5[7] = {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E};
    static const uint8_t D6[7] = {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E};
    static const uint8_t D7[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
    static const uint8_t D8[7] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
    static const uint8_t D9[7] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C};

    switch (c)
    {
        case 'B': return B;
        case 'C': return C;
        case 'I': return I;
        case 'N': return N;
        case 'S': return S;
        case ':': return COLON;
        case '0': return D0;
        case '1': return D1;
        case '2': return D2;
        case '3': return D3;
        case '4': return D4;
        case '5': return D5;
        case '6': return D6;
        case '7': return D7;
        case '8': return D8;
        case '9': return D9;
        case ' ': return SPACE;
        default: return SPACE;
    }
}

void sw_render_text5x7(uint16_t *dst, uint32_t w, uint32_t h,
                       int32_t x0, int32_t y0,
                       int32_t x, int32_t y, const char *s, uint16_t rgb565)
{
    if (!dst || !s) return;

    int32_t cx = x - x0;
    int32_t cy = y - y0;
    while (*s)
    {
        const uint8_t *g = sw_glyph5x7(*s);
        for (int32_t row = 0; row < 7; row++)
        {
            uint8_t bits = g[row];
            int32_t yy = cy + row;
            if ((uint32_t)yy >= h) continue;
            uint16_t *p = &dst[(uint32_t)yy * w];
            for (int32_t col = 0; col < 5; col++)
            {
                if (bits & (1u << (4 - col)))
                {
                    int32_t xx = cx + col;
                    if ((uint32_t)xx < w) p[(uint32_t)xx] = rgb565;
                }
            }
        }
        cx += 6;
        s++;
    }
}

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

static inline uint16_t sw_sample_dune_bg(int32_t gx, int32_t gy)
{
    if (gy < 0) gy = 0;
    uint32_t ty = ((uint32_t)gy) >> 1;
    if (ty >= DUNE_TEX_H) ty = DUNE_TEX_H - 1u;

    if (gx < 0) gx = 0;
    uint32_t tx = ((uint32_t)gx) >> 1;
    if (tx >= DUNE_TEX_W) tx = DUNE_TEX_W - 1u;

    return g_dune_tex[ty * DUNE_TEX_W + tx];
}

static inline uint8_t sw_luma_from_rgb565(uint16_t c)
{
    uint32_t r = (c >> 11) & 31u; r = (r << 3) | (r >> 2);
    uint32_t g = (c >> 5) & 63u;  g = (g << 2) | (g >> 4);
    uint32_t b = c & 31u;         b = (b << 3) | (b >> 2);
    uint32_t y = (r * 77u + g * 150u + b * 29u) >> 8;
    if (y > 255u) y = 255u;
    return (uint8_t)y;
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
                           int32_t cx, int32_t cy, int32_t r, uint32_t alpha_max)
{
    if (!dst || r <= 0) return;
    if (alpha_max > 255u) alpha_max = 255u;

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
            uint32_t a = (t * alpha_max) / 255u;
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
    if (!dst || r <= 0) return;

    int32_t sx0 = cx - r;
    int32_t sx1 = cx + r;
    int32_t sy0 = cy - r;
    int32_t sy1 = cy + r;

    /* Light direction (normalized-ish) in Q14, with a subtle wobble for moving highlights. */
    int32_t Lx = -6553;  /* -0.4 */
    int32_t Ly = -9830;  /* -0.6 */
    int32_t Lz = 11469;  /*  0.7 */
    {
        uint32_t tt = frame & 127u;
        int32_t tri = (tt < 64u) ? (int32_t)tt : (int32_t)(127u - tt); /* 0..63..0 */
        int32_t wave = tri - 32;                                      /* -32..31..-32 */
        int32_t wob = (wave * (int32_t)(400 + glint)) / 32;           /* Q14 delta */
        Lx = edgeai_clamp_i32(Lx + wob, -16000, 16000);
        Ly = edgeai_clamp_i32(Ly - (wob / 2), -16000, 16000);
        (void)Lz;
    }

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

            /* Specular: add a bright highlight and a background reflection (dune) for a ball bearing look. */
            int32_t env_q14 = spec + (fre >> 1);
            if (env_q14 > (1 << 14)) env_q14 = (1 << 14);
            if (env_q14 < 0) env_q14 = 0;

            int32_t refl_scale = r * 3;
            int32_t ex = cx + ((nx * refl_scale) >> 14);
            int32_t ey = cy + ((ny * refl_scale) >> 14);
            uint16_t env565 = sw_sample_dune_bg(ex, ey);
            uint32_t er = ((env565 >> 11) & 31u); er = (er << 3) | (er >> 2);
            uint32_t eg = ((env565 >> 5) & 63u);  eg = (eg << 2) | (eg >> 4);
            uint32_t eb = (env565 & 31u);         eb = (eb << 3) | (eb >> 2);

            r8 += (uint32_t)((er * (uint32_t)env_q14) >> 14);
            g8 += (uint32_t)((eg * (uint32_t)env_q14) >> 14);
            b8 += (uint32_t)((eb * (uint32_t)env_q14) >> 14);

            r8 += (uint32_t)((180u * (uint32_t)spec) >> 14);
            g8 += (uint32_t)((180u * (uint32_t)spec) >> 14);
            b8 += (uint32_t)((200u * (uint32_t)spec) >> 14);

            r8 += (uint32_t)((40u * (uint32_t)fre) >> 14);
            g8 += (uint32_t)((60u * (uint32_t)fre) >> 14);
            b8 += (uint32_t)((90u * (uint32_t)fre) >> 14);

            /* Moving "streak" highlight across the ball. */
            if (glint > 80u)
            {
                int32_t phase = (int32_t)((frame >> 1) & 31u) - 16;
                int32_t bw = 1 + (int32_t)(glint / 96u);
                int32_t band = (dx - dy + phase);
                if (band > -bw && band < bw)
                {
                    uint32_t a = (uint32_t)edgeai_clamp_i32((int32_t)glint - 60, 0, 255);
                    r8 += a / 6u;
                    g8 += a / 5u;
                    b8 += a / 4u;
                }
            }

            sw_put(dst, w, h, lx, ly, sw_pack_rgb565_u8(r8, g8, b8));
	        }
	    }
	}

void sw_render_ball_glow(uint16_t *dst, uint32_t w, uint32_t h,
                         int32_t x0, int32_t y0,
                         int32_t cx, int32_t cy, int32_t r, uint32_t frame, uint8_t glint)
{
    if (!dst || r <= 0) return;

    /* Fixed-weight, low-res post effect (placeholder for a Phase 1 NPU CNN). */
    enum { PP_W = 32, PP_H = 32, MAP_MAX = 128 };
    static uint8_t s_luma[PP_W * PP_H];
    static uint8_t s_edge[PP_W * PP_H];
    static uint8_t s_blur[PP_W * PP_H];

    const int32_t pad = 10;
    int32_t bx0 = cx - (r + pad);
    int32_t bx1 = cx + (r + pad);
    int32_t by0 = cy - (r + pad);
    int32_t by1 = cy + (r + pad);

    /* Clamp glow region to the tile buffer. */
    int32_t tx0 = x0;
    int32_t tx1 = x0 + (int32_t)w - 1;
    int32_t ty0 = y0;
    int32_t ty1 = y0 + (int32_t)h - 1;
    if (bx0 < tx0) bx0 = tx0;
    if (bx1 > tx1) bx1 = tx1;
    if (by0 < ty0) by0 = ty0;
    if (by1 > ty1) by1 = ty1;

    int32_t rw = bx1 - bx0 + 1;
    int32_t rh = by1 - by0 + 1;
    if (rw < 8 || rh < 8) return;
    if (rw > MAP_MAX || rh > MAP_MAX) return;

    /* Downsample luma from the tile into a small grid. */
    int32_t sx[PP_W];
    int32_t sy[PP_H];
    for (int32_t i = 0; i < PP_W; i++)
    {
        sx[i] = bx0 + (int32_t)(((int64_t)i * rw + (rw / 2)) / PP_W);
    }
    for (int32_t j = 0; j < PP_H; j++)
    {
        sy[j] = by0 + (int32_t)(((int64_t)j * rh + (rh / 2)) / PP_H);
    }
    for (int32_t j = 0; j < PP_H; j++)
    {
        int32_t gy = sy[j];
        uint32_t ly = (uint32_t)(gy - y0);
        for (int32_t i = 0; i < PP_W; i++)
        {
            int32_t gx = sx[i];
            uint32_t lx = (uint32_t)(gx - x0);
            s_luma[(uint32_t)j * PP_W + (uint32_t)i] = sw_luma_from_rgb565(dst[ly * w + lx]);
        }
    }

    /* Edge magnitude (Sobel). */
    for (int32_t j = 0; j < PP_H; j++)
    {
        for (int32_t i = 0; i < PP_W; i++)
        {
            if (i == 0 || j == 0 || i == (PP_W - 1) || j == (PP_H - 1))
            {
                s_edge[(uint32_t)j * PP_W + (uint32_t)i] = 0;
                continue;
            }

            int32_t idx = j * PP_W + i;
            int32_t l00 = (int32_t)s_luma[(uint32_t)(idx - PP_W - 1)];
            int32_t l01 = (int32_t)s_luma[(uint32_t)(idx - PP_W)];
            int32_t l02 = (int32_t)s_luma[(uint32_t)(idx - PP_W + 1)];
            int32_t l10 = (int32_t)s_luma[(uint32_t)(idx - 1)];
            int32_t l12 = (int32_t)s_luma[(uint32_t)(idx + 1)];
            int32_t l20 = (int32_t)s_luma[(uint32_t)(idx + PP_W - 1)];
            int32_t l21 = (int32_t)s_luma[(uint32_t)(idx + PP_W)];
            int32_t l22 = (int32_t)s_luma[(uint32_t)(idx + PP_W + 1)];

            int32_t gx = -l00 - (2 * l10) - l20 + l02 + (2 * l12) + l22;
            int32_t gy = -l00 - (2 * l01) - l02 + l20 + (2 * l21) + l22;
            int32_t mag = edgeai_abs_i32(gx) + edgeai_abs_i32(gy);
            if (mag > 255) mag = 255;
            s_edge[(uint32_t)j * PP_W + (uint32_t)i] = (uint8_t)mag;
        }
    }

    /* Small blur to turn edges into glow. */
    for (int32_t j = 0; j < PP_H; j++)
    {
        for (int32_t i = 0; i < PP_W; i++)
        {
            uint32_t sum = 0;
            for (int32_t dj = -1; dj <= 1; dj++)
            {
                int32_t y = j + dj;
                if ((uint32_t)y >= PP_H) continue;
                for (int32_t di = -1; di <= 1; di++)
                {
                    int32_t x = i + di;
                    if ((uint32_t)x >= PP_W) continue;
                    sum += (uint32_t)s_edge[(uint32_t)y * PP_W + (uint32_t)x];
                }
            }
            sum /= 9u;
            if (sum > 255u) sum = 255u;
            s_blur[(uint32_t)j * PP_W + (uint32_t)i] = (uint8_t)sum;
        }
    }

    /* Precompute high-res -> low-res index maps. */
    uint8_t mx[MAP_MAX];
    uint8_t my[MAP_MAX];
    for (int32_t x = 0; x < rw; x++) mx[x] = (uint8_t)((x * PP_W) / rw);
    for (int32_t y = 0; y < rh; y++) my[y] = (uint8_t)((y * PP_H) / rh);

    /* Time-varying shimmer multiplier. */
    uint32_t tt = (frame >> 2) & 63u;
    uint32_t tri = (tt < 32u) ? tt : (63u - tt); /* 0..31..0 */
    uint32_t shimmer = 200u + tri;               /* 200..231 */

    for (int32_t gy = by0; gy <= by1; gy++)
    {
        uint32_t ly = (uint32_t)(gy - y0);
        uint8_t iy = my[gy - by0];
        for (int32_t gx = bx0; gx <= bx1; gx++)
        {
            uint8_t ix = mx[gx - bx0];
            uint32_t g = (uint32_t)s_blur[(uint32_t)iy * PP_W + (uint32_t)ix];
            if (g <= 24u) continue;

            uint32_t a = g - 24u;
            a = (a * (shimmer + (uint32_t)glint)) >> 8;
            if (a == 0u) continue;

            uint32_t lx = (uint32_t)(gx - x0);
            uint16_t c = dst[ly * w + lx];
            uint32_t r5 = (c >> 11) & 31u;
            uint32_t g6 = (c >> 5) & 63u;
            uint32_t b5 = c & 31u;
            uint32_t r8 = (r5 << 3) | (r5 >> 2);
            uint32_t g8 = (g6 << 2) | (g6 >> 4);
            uint32_t b8 = (b5 << 3) | (b5 >> 2);

            r8 += a / 6u;
            g8 += a / 5u;
            b8 += a / 4u;
            dst[ly * w + lx] = sw_pack_rgb565_u8(r8, g8, b8);
        }
    }
}
