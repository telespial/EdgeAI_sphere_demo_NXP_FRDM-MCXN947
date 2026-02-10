#include "par_lcd_s035.h"

#include <string.h>

#include "fsl_clock.h"
#include "fsl_common.h"
#include "fsl_debug_console.h"
#include "fsl_gpio.h"

#include "fsl_dbi_flexio_edma.h"
#include "fsl_flexio_mculcd.h"
#include "fsl_st7796s.h"

#include "board.h"
#include "pin_mux.h"

/* NXP PAR-LCD-S035 (ST7796S, 480x320, 8080 via FlexIO0). */
/* Note: this driver uses a single-buffer update path.
 * DMA, double-buffering, or alternative partial-redraw strategies should be implemented here.
 */
#define EDGEAI_LCD_WIDTH  480u
#define EDGEAI_LCD_HEIGHT 320u

#define EDGEAI_LCD_RST_GPIO GPIO4
#define EDGEAI_LCD_RST_PIN  7u
#define EDGEAI_LCD_CS_GPIO  GPIO0
#define EDGEAI_LCD_CS_PIN   12u
#define EDGEAI_LCD_RS_GPIO  GPIO0
#define EDGEAI_LCD_RS_PIN   7u

#define EDGEAI_FLEXIO               FLEXIO0
#define EDGEAI_FLEXIO_CLOCK_FREQ    CLOCK_GetFlexioClkFreq()
#define EDGEAI_FLEXIO_BAUDRATE_BPS  160000000u

#define EDGEAI_FLEXIO_WR_PIN            1u
#define EDGEAI_FLEXIO_RD_PIN            0u
#define EDGEAI_FLEXIO_DATA_PIN_START    16u
#define EDGEAI_FLEXIO_TX_START_SHIFTER  0u
#define EDGEAI_FLEXIO_TX_END_SHIFTER    7u
#define EDGEAI_FLEXIO_RX_START_SHIFTER  0u
#define EDGEAI_FLEXIO_RX_END_SHIFTER    7u
#define EDGEAI_FLEXIO_TIMER             0u

static volatile bool s_memWriteDone = false;
static st7796s_handle_t s_lcdHandle;
static dbi_flexio_edma_xfer_handle_t s_dbiFlexioXferHandle;

static void edgeai_set_cs(bool set)
{
    GPIO_PinWrite(EDGEAI_LCD_CS_GPIO, EDGEAI_LCD_CS_PIN, set ? 1u : 0u);
}

static void edgeai_set_rs(bool set)
{
    GPIO_PinWrite(EDGEAI_LCD_RS_GPIO, EDGEAI_LCD_RS_PIN, set ? 1u : 0u);
}

static void edgeai_dbi_done_cb(status_t status, void *userData)
{
    (void)status;
    (void)userData;
    s_memWriteDone = true;
}

static FLEXIO_MCULCD_Type s_flexioLcdDev = {
    .flexioBase          = EDGEAI_FLEXIO,
    .busType             = kFLEXIO_MCULCD_8080,
    .dataPinStartIndex   = EDGEAI_FLEXIO_DATA_PIN_START,
    .ENWRPinIndex        = EDGEAI_FLEXIO_WR_PIN,
    .RDPinIndex          = EDGEAI_FLEXIO_RD_PIN,
    .txShifterStartIndex = EDGEAI_FLEXIO_TX_START_SHIFTER,
    .txShifterEndIndex   = EDGEAI_FLEXIO_TX_END_SHIFTER,
    .rxShifterStartIndex = EDGEAI_FLEXIO_RX_START_SHIFTER,
    .rxShifterEndIndex   = EDGEAI_FLEXIO_RX_END_SHIFTER,
    .timerIndex          = EDGEAI_FLEXIO_TIMER,
    .setCSPin            = edgeai_set_cs,
    .setRSPin            = edgeai_set_rs,
    .setRDWRPin          = NULL /* Not used in 8080 mode. */
};

static void edgeai_delay_ms(uint32_t ms)
{
    SDK_DelayAtLeastUs(ms * 1000u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
}

static void lcd_wait_write_done(void)
{
    /* Guard against rare EDMA/callback stalls; prefer a visible glitch over a hard hang. */
    uint32_t spin = 0;
    while (!s_memWriteDone)
    {
        if (++spin > 60000000u) /* ~0.4s @ 150MHz ballpark */
        {
            /* Force progress. The next frame will likely recover. */
            s_memWriteDone = true;
            break;
        }
        __NOP();
    }
}

bool par_lcd_s035_init(void)
{
    BOARD_InitLcdPins();

    flexio_mculcd_config_t flexioCfg;
    FLEXIO_MCULCD_GetDefaultConfig(&flexioCfg);
    flexioCfg.baudRate_Bps = EDGEAI_FLEXIO_BAUDRATE_BPS;

    status_t st = FLEXIO_MCULCD_Init(&s_flexioLcdDev, &flexioCfg, EDGEAI_FLEXIO_CLOCK_FREQ);
    if (st != kStatus_Success)
    {
        PRINTF("LCD: FLEXIO_MCULCD_Init failed: %d\r\n", (int)st);
        return false;
    }

    st = DBI_FLEXIO_EDMA_CreateXferHandle(&s_dbiFlexioXferHandle, &s_flexioLcdDev, NULL, NULL);
    if (st != kStatus_Success)
    {
        PRINTF("LCD: DBI_FLEXIO_EDMA_CreateXferHandle failed: %d\r\n", (int)st);
        return false;
    }

    /* Hardware reset. */
    GPIO_PinWrite(EDGEAI_LCD_RST_GPIO, EDGEAI_LCD_RST_PIN, 0u);
    edgeai_delay_ms(2);
    GPIO_PinWrite(EDGEAI_LCD_RST_GPIO, EDGEAI_LCD_RST_PIN, 1u);
    edgeai_delay_ms(10);

    const st7796s_config_t cfg = {
        .driverPreset    = kST7796S_DriverPresetLCDPARS035,
        .pixelFormat     = kST7796S_PixelFormatRGB565,
        .orientationMode = kST7796S_Orientation270,
        .teConfig        = kST7796S_TEDisabled,
        .invertDisplay   = true,
        .flipDisplay     = true,
        .bgrFilter       = true,
    };

    st = ST7796S_Init(&s_lcdHandle, &cfg, &g_dbiFlexioEdmaXferOps, &s_dbiFlexioXferHandle);
    if (st != kStatus_Success)
    {
        PRINTF("LCD: ST7796S_Init failed: %d\r\n", (int)st);
        return false;
    }

    ST7796S_SetMemoryDoneCallback(&s_lcdHandle, edgeai_dbi_done_cb, NULL);
    ST7796S_EnableDisplay(&s_lcdHandle, true);
    return true;
}

void par_lcd_s035_fill(uint16_t rgb565)
{
    static uint16_t line[EDGEAI_LCD_WIDTH];
    for (uint32_t i = 0; i < EDGEAI_LCD_WIDTH; i++)
    {
        line[i] = rgb565;
    }

    for (uint32_t y = 0; y < EDGEAI_LCD_HEIGHT; y++)
    {
        ST7796S_SelectArea(&s_lcdHandle, 0, (uint16_t)y, EDGEAI_LCD_WIDTH - 1u, (uint16_t)y);
        s_memWriteDone = false;
        ST7796S_WritePixels(&s_lcdHandle, line, EDGEAI_LCD_WIDTH);
        lcd_wait_write_done();
    }
}

void par_lcd_s035_blit_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t *rgb565)
{
    if (!rgb565) return;
    if (x1 < x0 || y1 < y0) return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= (int32_t)EDGEAI_LCD_WIDTH) x1 = (int32_t)EDGEAI_LCD_WIDTH - 1;
    if (y1 >= (int32_t)EDGEAI_LCD_HEIGHT) y1 = (int32_t)EDGEAI_LCD_HEIGHT - 1;

    uint32_t w = (uint32_t)(x1 - x0 + 1);
    uint32_t h = (uint32_t)(y1 - y0 + 1);
    uint32_t n = w * h;

    ST7796S_SelectArea(&s_lcdHandle, (uint16_t)x0, (uint16_t)y0, (uint16_t)x1, (uint16_t)y1);
    s_memWriteDone = false;
    ST7796S_WritePixels(&s_lcdHandle, rgb565, n);
    lcd_wait_write_done();
}

static void lcd_fill_span(uint16_t x0, uint16_t y, uint16_t x1, uint16_t color)
{
    if (x1 < x0) return;
    uint32_t w = (uint32_t)(x1 - x0 + 1u);
    static uint16_t buf[EDGEAI_LCD_WIDTH];
    if (w > EDGEAI_LCD_WIDTH) w = EDGEAI_LCD_WIDTH;
    for (uint32_t i = 0; i < w; i++) buf[i] = color;

    ST7796S_SelectArea(&s_lcdHandle, x0, y, x1, y);
    s_memWriteDone = false;
    ST7796S_WritePixels(&s_lcdHandle, buf, w);
    lcd_wait_write_done();
}

void par_lcd_s035_draw_filled_circle(int32_t cx, int32_t cy, int32_t r, uint16_t rgb565)
{
    if (r <= 0) return;

    int32_t y0 = cy - r;
    int32_t y1 = cy + r;
    if (y0 < 0) y0 = 0;
    if (y1 >= (int32_t)EDGEAI_LCD_HEIGHT) y1 = (int32_t)EDGEAI_LCD_HEIGHT - 1;

    for (int32_t y = y0; y <= y1; y++)
    {
        int32_t dy = y - cy;
        int32_t dy2 = dy * dy;
        int32_t dx_max = 0;

        /* r is small; a short scan is fine and keeps code simple/deterministic. */
        for (int32_t dx = 0; dx <= r; dx++)
        {
            if (dx * dx + dy2 > r * r)
            {
                dx_max = dx - 1;
                break;
            }
            dx_max = dx;
        }

        int32_t x0 = cx - dx_max;
        int32_t x1 = cx + dx_max;
        if (x0 < 0) x0 = 0;
        if (x1 >= (int32_t)EDGEAI_LCD_WIDTH) x1 = (int32_t)EDGEAI_LCD_WIDTH - 1;

        lcd_fill_span((uint16_t)x0, (uint16_t)y, (uint16_t)x1, rgb565);
    }
}

void par_lcd_s035_fill_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t rgb565)
{
    if (x1 < x0 || y1 < y0) return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= (int32_t)EDGEAI_LCD_WIDTH) x1 = (int32_t)EDGEAI_LCD_WIDTH - 1;
    if (y1 >= (int32_t)EDGEAI_LCD_HEIGHT) y1 = (int32_t)EDGEAI_LCD_HEIGHT - 1;

    uint32_t w = (uint32_t)(x1 - x0 + 1);
    static uint16_t buf[EDGEAI_LCD_WIDTH];
    if (w > EDGEAI_LCD_WIDTH) w = EDGEAI_LCD_WIDTH;
    for (uint32_t i = 0; i < w; i++) buf[i] = rgb565;

    for (int32_t y = y0; y <= y1; y++)
    {
        ST7796S_SelectArea(&s_lcdHandle, (uint16_t)x0, (uint16_t)y, (uint16_t)x1, (uint16_t)y);
        s_memWriteDone = false;
        ST7796S_WritePixels(&s_lcdHandle, buf, w);
        lcd_wait_write_done();
    }
}

static inline uint16_t pack_rgb565_u8(uint32_t r8, uint32_t g8, uint32_t b8)
{
    if (r8 > 255u) r8 = 255u;
    if (g8 > 255u) g8 = 255u;
    if (b8 > 255u) b8 = 255u;
    uint16_t r = (uint16_t)(r8 >> 3);
    uint16_t g = (uint16_t)(g8 >> 2);
    uint16_t b = (uint16_t)(b8 >> 3);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static inline uint32_t isqrt_u32(uint32_t x)
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

static inline uint32_t xorshift32(uint32_t x)
{
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

static inline uint8_t noise_u8(uint32_t x, uint32_t y, uint32_t seed)
{
    /* Cheap 2D hash -> 8-bit noise. */
    uint32_t n = (x * 0x9E3779B1u) ^ (y * 0x85EBCA77u) ^ seed;
    n = xorshift32(n);
    return (uint8_t)(n >> 24);
}

void par_lcd_s035_draw_ball_shadow(int32_t cx, int32_t cy, int32_t r, uint32_t alpha_max)
{
    /* Not a physical shadow on black; it's a soft AO spot to add depth. */
    if (alpha_max > 255u) alpha_max = 255u;
    int32_t sh_cx = cx + (r / 4);
    int32_t sh_cy = cy + r + (r / 2) + 8;
    int32_t rx = r + 18;
    int32_t ry = (r / 2) + 10;

    int32_t y0 = sh_cy - ry;
    int32_t y1 = sh_cy + ry;
    if (y0 < 0) y0 = 0;
    if (y1 >= (int32_t)EDGEAI_LCD_HEIGHT) y1 = (int32_t)EDGEAI_LCD_HEIGHT - 1;

    static uint16_t line[EDGEAI_LCD_WIDTH];
    for (int32_t y = y0; y <= y1; y++)
    {
        int32_t dy = y - sh_cy;
        uint32_t dy2 = (uint32_t)(dy * dy);

        int32_t x0 = sh_cx - rx;
        int32_t x1 = sh_cx + rx;
        if (x0 < 0) x0 = 0;
        if (x1 >= (int32_t)EDGEAI_LCD_WIDTH) x1 = (int32_t)EDGEAI_LCD_WIDTH - 1;

        uint32_t w = (uint32_t)(x1 - x0 + 1);
        if (w > EDGEAI_LCD_WIDTH) w = EDGEAI_LCD_WIDTH;
        memset(line, 0, w * sizeof(line[0])); /* background is black */

        for (int32_t x = x0; x <= x1; x++)
        {
            int32_t dx = x - sh_cx;
            uint32_t dx2 = (uint32_t)(dx * dx);
            /* ellipse distance in Q8: (dx^2/rx^2)+(dy^2/ry^2) */
            uint32_t d = (dx2 * 256u) / (uint32_t)(rx * rx) + (dy2 * 256u) / (uint32_t)(ry * ry);
            if (d >= 256u) continue;

            uint32_t t = 256u - d; /* 0..255 */
            /* AO spot is a faint bluish-gray so it's visible on black. */
            uint32_t a = (t * alpha_max) / 255u;
            uint16_t c = pack_rgb565_u8(a, a, a + (a / 3));
            line[(uint32_t)(x - x0)] = c;
        }

        ST7796S_SelectArea(&s_lcdHandle, (uint16_t)x0, (uint16_t)y, (uint16_t)x1, (uint16_t)y);
        s_memWriteDone = false;
        ST7796S_WritePixels(&s_lcdHandle, line, w);
        lcd_wait_write_done();
    }
}

void par_lcd_s035_draw_silver_ball(int32_t cx, int32_t cy, int32_t r,
                                   uint32_t phase, uint8_t glint,
                                   int32_t spin_sin_q14, int32_t spin_cos_q14)
{
    if (r <= 0) return;

    /* Ray-traced sphere shading for a single object (analytic ray/sphere). */
    const int32_t x0 = (cx - r < 0) ? 0 : (cx - r);
    const int32_t x1 = (cx + r >= (int32_t)EDGEAI_LCD_WIDTH) ? (int32_t)EDGEAI_LCD_WIDTH - 1 : (cx + r);
    const int32_t y0 = (cy - r < 0) ? 0 : (cy - r);
    const int32_t y1 = (cy + r >= (int32_t)EDGEAI_LCD_HEIGHT) ? (int32_t)EDGEAI_LCD_HEIGHT - 1 : (cy + r);

    static uint16_t line[EDGEAI_LCD_WIDTH];

    /* Light direction (normalized-ish) in Q14. */
    const int32_t Lx = -6553;  /* -0.4 */
    const int32_t Ly = -9830;  /* -0.6 */
    const int32_t Lz = 11469;  /*  0.7 */

    const uint32_t r2 = (uint32_t)(r * r);
    const uint32_t seed = (phase * 0xA511E9B3u) ^ ((uint32_t)glint * 0x63D83595u);
    const uint32_t off_u = (phase >> 3) & 255u;
    const uint32_t off_v = (phase >> 4) & 255u;

    for (int32_t y = y0; y <= y1; y++)
    {
        uint32_t w = (uint32_t)(x1 - x0 + 1);
        if (w > EDGEAI_LCD_WIDTH) w = EDGEAI_LCD_WIDTH;
        memset(line, 0, w * sizeof(line[0])); /* background black */

        int32_t dy = y - cy;
        int32_t dy2 = dy * dy;
        if ((uint32_t)dy2 > r2)
        {
            /* no pixels in this row */
            goto write_row;
        }

        /* Compute max dx for this row. */
        uint32_t dx_max = isqrt_u32(r2 - (uint32_t)dy2);
        int32_t sx0 = cx - (int32_t)dx_max;
        int32_t sx1 = cx + (int32_t)dx_max;
        if (sx0 < x0) sx0 = x0;
        if (sx1 > x1) sx1 = x1;

        for (int32_t x = sx0; x <= sx1; x++)
        {
            int32_t dx = x - cx;
            uint32_t d2 = (uint32_t)(dx * dx + dy2);
            if (d2 > r2) continue;

            /* z = sqrt(r^2 - x^2 - y^2) */
            uint32_t zz = isqrt_u32(r2 - d2);

            /* Normal in Q14: n = (dx,dy,z)/r. */
            int32_t nx = (dx << 14) / r;
            int32_t ny = (dy << 14) / r;
            int32_t nz = ((int32_t)zz << 14) / r;

            /* Diffuse = max(dot(n, L), 0) in Q14. */
            int32_t ndl = (nx * Lx + ny * Ly + nz * Lz) >> 14;
            if (ndl < 0) ndl = 0;

            /* View is (0,0,1). Spec approx: (max(Rz,0))^16. */
            /* R = 2*ndl*n - L */
            int32_t rz = ((2 * ndl * nz) >> 14) - Lz;
            if (rz < 0) rz = 0;

            /* Normalize-ish spec base to [0..1] Q14 by dividing by 1.0*Q14 (approx). */
            int32_t s = rz;
            /* pow16 */
            int32_t s2 = (s * s) >> 14;
            int32_t s4 = (s2 * s2) >> 14;
            int32_t s8 = (s4 * s4) >> 14;
            int32_t s16 = (s8 * s8) >> 14;

            /* Fresnel term ~ (1-nz)^5, nz in Q14. */
            int32_t inv = (1 << 14) - nz;
            if (inv < 0) inv = 0;
            int32_t f2 = (inv * inv) >> 14;
            int32_t f4 = (f2 * f2) >> 14;
            int32_t f5 = (f4 * inv) >> 14;

            /* Base "silver" tint (kept slightly dark; reflections add punch). */
            uint32_t br = 150, bg = 155, bb = 165;

            /* Lighting combine (all Q14), integer-only.
             * glint scales specular to make the ball feel more "alive".
             */
            const int32_t amb = 1966;          /* 0.12 * 16384 */
            const int32_t diff_k = 9011;       /* 0.55 * 16384 */
            int32_t spec_k = 16384;            /* 1.00 * 16384 */
            const int32_t fre_k  = 4096;       /* 0.25 * 16384 */
            spec_k += (int32_t)((uint32_t)glint * 8192u / 255u); /* +0..0.5 */
            int32_t diff = (ndl * diff_k) >> 14;
            int32_t spec = (s16 * spec_k) >> 14;
            int32_t fre  = (f5  * fre_k) >> 14;
            int32_t I = amb + diff;
            if (I > (2 << 14)) I = (2 << 14);

            /* Apply diffuse/ambient to base. */
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

            /* Reflectivity (Fresnel-ish). */
            int32_t refl_k = 8192 + ((f5 * 8192) >> 14); /* 0.5..1.0 */
            r8 += (env_r * (uint32_t)refl_k) >> 14;
            g8 += (env_g * (uint32_t)refl_k) >> 14;
            b8 += (env_b * (uint32_t)refl_k) >> 14;

            /* Moving sparkles in the specular highlight region (replaces the static 45-degree streak). */
            if (glint > 12u && s16 > 2500)
            {
                int32_t ux = (nx * spin_cos_q14 - ny * spin_sin_q14) >> 14;
                int32_t uy = (nx * spin_sin_q14 + ny * spin_cos_q14) >> 14;
                uint32_t iu = ((uint32_t)(ux + (1 << 14)) >> 7) + off_u;
                uint32_t iv = ((uint32_t)(uy + (1 << 14)) >> 7) + off_v;
                uint8_t n = noise_u8(iu & 255u, iv & 255u, seed);
                int32_t thresh = 252 - ((int32_t)glint >> 4); /* 252..237 */
                if ((int32_t)n > thresh)
                {
                    int32_t d = (int32_t)n - thresh;
                    int32_t sparkle_q14 = d << 10;
                    if (sparkle_q14 > (1 << 14)) sparkle_q14 = (1 << 14);
                    sparkle_q14 = (sparkle_q14 * s16) >> 14;
                    spec += sparkle_q14;
                }
            }

            /* Add specular (white) and fresnel (cool tint). */
            r8 += (uint32_t)((255u * (uint32_t)spec) >> 14);
            g8 += (uint32_t)((255u * (uint32_t)spec) >> 14);
            b8 += (uint32_t)((255u * (uint32_t)spec) >> 14);

            r8 += (uint32_t)((40u * (uint32_t)fre) >> 14);
            g8 += (uint32_t)((60u * (uint32_t)fre) >> 14);
            b8 += (uint32_t)((90u * (uint32_t)fre) >> 14);

            line[(uint32_t)(x - x0)] = pack_rgb565_u8(r8, g8, b8);
        }

write_row:
        ST7796S_SelectArea(&s_lcdHandle, (uint16_t)x0, (uint16_t)y, (uint16_t)x1, (uint16_t)y);
        s_memWriteDone = false;
        ST7796S_WritePixels(&s_lcdHandle, line, w);
        lcd_wait_write_done();
    }
}
