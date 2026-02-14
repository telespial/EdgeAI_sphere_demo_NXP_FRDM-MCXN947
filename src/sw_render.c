#include "sw_render.h"

#include <string.h>

#include "dune_bg.h"

static const uint8_t *sw_glyph5x7(char c)
{
    static const uint8_t SPACE[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t COLON[7] = {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00};

    static const uint8_t B[7] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
    static const uint8_t A[7] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    static const uint8_t C[7] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
    static const uint8_t D[7] = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
    static const uint8_t E[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
    static const uint8_t H[7] = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    static const uint8_t I[7] = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
    static const uint8_t K[7] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
    static const uint8_t N[7] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
    static const uint8_t R[7] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
    static const uint8_t S[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    static const uint8_t LP[7] = {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02};
    static const uint8_t RP[7] = {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08};

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
        case 'A': return A;
        case 'B': return B;
        case 'C': return C;
        case 'D': return D;
        case 'E': return E;
        case 'H': return H;
        case 'I': return I;
        case 'K': return K;
        case 'N': return N;
        case 'R': return R;
        case 'S': return S;
        case 'c': return C;
        case '(': return LP;
        case ')': return RP;
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

static inline uint32_t sw_xorshift32(uint32_t x)
{
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

static inline uint8_t sw_noise_u8(uint32_t x, uint32_t y, uint32_t seed)
{
    /* Cheap 2D hash -> 8-bit noise. */
    uint32_t n = (x * 0x9E3779B1u) ^ (y * 0x85EBCA77u) ^ seed;
    n = sw_xorshift32(n);
    return (uint8_t)(n >> 24);
}

void sw_render_silver_ball(uint16_t *dst, uint32_t w, uint32_t h,
                           int32_t x0, int32_t y0,
                           int32_t cx, int32_t cy, int32_t r,
                           uint32_t phase, uint8_t glint,
                           int32_t spin_sin_q14, int32_t spin_cos_q14)
{
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
    const uint32_t seed = (phase * 0xA511E9B3u) ^ ((uint32_t)glint * 0x63D83595u);
    const uint32_t off_u = (phase >> 3) & 255u;
    const uint32_t off_v = (phase >> 4) & 255u;

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

            /* Base "silver" tint (kept slightly dark; reflections add punch). */
            uint32_t br = 150, bg = 155, bb = 165;

            const int32_t amb = 1966;     /* 0.12 * 16384 */
            const int32_t diff_k = 9011;  /* 0.55 * 16384 */
            int32_t spec_k = 16384;       /* 1.00 * 16384 */
            const int32_t fre_k  = 4096;  /* 0.25 * 16384 */
            spec_k += (int32_t)((uint32_t)glint * 8192u / 255u); /* +0..0.5 */

            int32_t diff = (ndl * diff_k) >> 14;
            int32_t spec = (s16 * spec_k) >> 14;
            int32_t fre  = (f5  * fre_k) >> 14;
            int32_t I = amb + diff;
            if (I > (2 << 14)) I = (2 << 14);

            uint32_t r8 = (br * (uint32_t)I) >> 14;
            uint32_t g8 = (bg * (uint32_t)I) >> 14;
            uint32_t b8 = (bb * (uint32_t)I) >> 14;

            /* Environment reflection (very cheap "sky + ground + sun" model), rotated by spin. */
            int32_t Rx = (2 * ((nz * nx) >> 14));
            int32_t Ry = (2 * ((nz * ny) >> 14));
            int32_t Rz = (((2 * ((nz * nz) >> 14)) - (1 << 14)));
            int32_t Rxr = (Rx * spin_cos_q14 - Ry * spin_sin_q14) >> 14;
            int32_t Ryr = (Rx * spin_sin_q14 + Ry * spin_cos_q14) >> 14;

            int32_t t = (Rz + (1 << 14)) >> 1; /* 0..16384 */
            if (t < 0) t = 0;
            if (t > (1 << 14)) t = (1 << 14);

            const uint32_t sky_r = 90,  sky_g = 135, sky_b = 180;
            const uint32_t grd_r = 160, grd_g = 120, grd_b = 80;
            uint32_t env_r = (grd_r * (uint32_t)((1 << 14) - t) + sky_r * (uint32_t)t) >> 14;
            uint32_t env_g = (grd_g * (uint32_t)((1 << 14) - t) + sky_g * (uint32_t)t) >> 14;
            uint32_t env_b = (grd_b * (uint32_t)((1 << 14) - t) + sky_b * (uint32_t)t) >> 14;

            /* Add a small sun spot in the sky. */
            int32_t sun_dot = (Rxr * Lx + Ryr * Ly + Rz * Lz) >> 14;
            if (sun_dot < 0) sun_dot = 0;
            int32_t sd2 = (sun_dot * sun_dot) >> 14;
            int32_t sd4 = (sd2 * sd2) >> 14;
            int32_t sd8 = (sd4 * sd4) >> 14;
            int32_t sd16 = (sd8 * sd8) >> 14;
            uint32_t sun_k = (uint32_t)((sd16 * 220) >> 14); /* 0..220 */
            env_r += sun_k;
            env_g += (sun_k * 210u) / 220u;
            env_b += (sun_k * 170u) / 220u;

            /* Reflectivity (Fresnel-ish) and a little speed-based punch via glint. */
            int32_t refl_k = 8192 + ((f5 * 8192) >> 14); /* 0.5..1.0 */
            r8 += (env_r * (uint32_t)refl_k) >> 14;
            g8 += (env_g * (uint32_t)refl_k) >> 14;
            b8 += (env_b * (uint32_t)refl_k) >> 14;

            /* Moving sparkles in the specular highlight region (replaces the static 45-degree streak). */
            if (glint > 12u && s16 > 2500)
            {
                /* Rotate tangent coords and slide them with phase to simulate rolling. */
                int32_t ux = (nx * spin_cos_q14 - ny * spin_sin_q14) >> 14;
                int32_t uy = (nx * spin_sin_q14 + ny * spin_cos_q14) >> 14;
                uint32_t iu = ((uint32_t)(ux + (1 << 14)) >> 7) + off_u; /* ~0..510 */
                uint32_t iv = ((uint32_t)(uy + (1 << 14)) >> 7) + off_v;
                uint8_t n = sw_noise_u8(iu & 255u, iv & 255u, seed);
                int32_t thresh = 252 - ((int32_t)glint >> 4); /* 252..237 */
                if ((int32_t)n > thresh)
                {
                    int32_t d = (int32_t)n - thresh; /* 1.. */
                    int32_t sparkle_q14 = d << 10;   /* 0..~18432 */
                    if (sparkle_q14 > (1 << 14)) sparkle_q14 = (1 << 14);
                    sparkle_q14 = (sparkle_q14 * s16) >> 14;
                    spec += sparkle_q14;
                }
            }

            r8 += (uint32_t)((255u * (uint32_t)spec) >> 14);
            g8 += (uint32_t)((255u * (uint32_t)spec) >> 14);
            b8 += (uint32_t)((255u * (uint32_t)spec) >> 14);

            r8 += (uint32_t)((40u * (uint32_t)fre) >> 14);
            g8 += (uint32_t)((60u * (uint32_t)fre) >> 14);
            b8 += (uint32_t)((90u * (uint32_t)fre) >> 14);

            sw_put(dst, w, h, lx, ly, sw_pack_rgb565_u8(r8, g8, b8));
        }
    }
}
