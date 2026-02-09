#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "accel4_click.h"
#include "fxls8974cf.h"
#include "par_lcd_s035.h"
#include "sw_render.h"
#include "npu/model.h"

#include "app.h"
#include "board.h"
#include "clock_config.h"
#include "fsl_clock.h"
#include "fsl_debug_console.h"
#include "fsl_lpi2c.h"
#include "pin_mux.h"

#define LCD_W 480
#define LCD_H 320

/* Ball radius tuning.
 * We apply a simple depth cue: ball gets smaller toward the top ("far") and larger toward the
 * bottom ("near") to better match the dune background's perspective.
 */
#define BALL_R_BASE 20
#define BALL_R_MIN  12
#define BALL_R_MAX  34

static int32_t edgeai_ball_r_for_y(int32_t cy)
{
    /* Map screen y to [0..256] where 0 is "far" and 256 is "near". */
    const int32_t y_far = 26;
    const int32_t y_near = LCD_H - 26;
    int32_t denom = (y_near - y_far);
    int32_t t_q8 = 256;
    if (denom > 0)
    {
        int32_t num = cy - y_far;
        if (num < 0) num = 0;
        if (num > denom) num = denom;
        t_q8 = (num * 256) / denom;
    }

    /* Radius ramps from MIN..MAX. */
    int32_t r = BALL_R_MIN + ((t_q8 * (BALL_R_MAX - BALL_R_MIN)) / 256);
    if (r < BALL_R_MIN) r = BALL_R_MIN;
    if (r > BALL_R_MAX) r = BALL_R_MAX;
    (void)BALL_R_BASE;
    return r;
}

/* Empirically, the raw 12-bit output here is roughly on the order of ~512 counts per 1g
 * at the current FXLS8974 config. We use this as the full-scale tilt mapping value.
 */
#define ACCEL_MAP_DENOM 512

/* Accel axis mapping (adjust if needed). */
#ifndef EDGEAI_ACCEL_SWAP_XY
#define EDGEAI_ACCEL_SWAP_XY 1
#endif
#ifndef EDGEAI_ACCEL_INVERT_X
#define EDGEAI_ACCEL_INVERT_X 0
#endif
#ifndef EDGEAI_ACCEL_INVERT_Y
#define EDGEAI_ACCEL_INVERT_Y 0
#endif

#ifndef EDGEAI_I2C
#define EDGEAI_I2C LPI2C3
#endif

/* NPU: keep init enabled, but inference can be disabled if it blocks on a given setup. */
#ifndef EDGEAI_ENABLE_NPU_INFERENCE
#define EDGEAI_ENABLE_NPU_INFERENCE 0
#endif

/* Rendering mode:
 * - 0: "raster/flicker" mode: draw primitives directly to LCD using many small writes
 *      (visually interesting but can show tearing/shutter lines).
 * - 1: "single blit" mode: render into a RAM tile and blit once per frame
 *      (stable image, much less tearing).
 */
#ifndef EDGEAI_RENDER_SINGLE_BLIT
#define EDGEAI_RENDER_SINGLE_BLIT 1
#endif

static uint32_t edgeai_i2c_get_freq(void)
{
    return CLOCK_GetLPFlexCommClkFreq(3u);
}

static bool edgeai_i2c_write(uint8_t addr7, uint8_t reg, const uint8_t *data, uint32_t len)
{
    lpi2c_master_transfer_t xfer;
    memset(&xfer, 0, sizeof(xfer));
    xfer.slaveAddress = addr7;
    xfer.direction = kLPI2C_Write;
    xfer.subaddress = reg;
    xfer.subaddressSize = 1;
    xfer.data = (uint8_t *)data;
    xfer.dataSize = len;
    xfer.flags = kLPI2C_TransferDefaultFlag;
    return (LPI2C_MasterTransferBlocking(EDGEAI_I2C, &xfer) == kStatus_Success);
}

static bool edgeai_i2c_read(uint8_t addr7, uint8_t reg, uint8_t *data, uint32_t len)
{
    lpi2c_master_transfer_t xfer;
    memset(&xfer, 0, sizeof(xfer));
    xfer.slaveAddress = addr7;
    xfer.direction = kLPI2C_Read;
    xfer.subaddress = reg;
    xfer.subaddressSize = 1;
    xfer.data = data;
    xfer.dataSize = len;
    xfer.flags = kLPI2C_TransferDefaultFlag;
    return (LPI2C_MasterTransferBlocking(EDGEAI_I2C, &xfer) == kStatus_Success);
}

static void map_accel_xy(int32_t *ax, int32_t *ay)
{
    if (!ax || !ay) return;
#if EDGEAI_ACCEL_SWAP_XY
    int32_t tmp = *ax; *ax = *ay; *ay = tmp;
#endif
#if EDGEAI_ACCEL_INVERT_X
    *ax = -*ax;
#endif
#if EDGEAI_ACCEL_INVERT_Y
    *ay = -*ay;
#endif
}

static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void dwt_cycle_counter_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static int32_t abs_i32(int32_t v) { return (v < 0) ? -v : v; }

static void edgeai_u32_to_dec3(char out[4], uint32_t v)
{
    if (v > 999u) v = 999u;
    out[0] = (char)('0' + (v / 100u));
    out[1] = (char)('0' + ((v / 10u) % 10u));
    out[2] = (char)('0' + (v % 10u));
    out[3] = '\0';
}

static int32_t clamp_i32_sym(int32_t v, int32_t limit_abs)
{
    if (v > limit_abs) return limit_abs;
    if (v < -limit_abs) return -limit_abs;
    return v;
}

static const uint8_t *edgeai_glyph5x7(char c)
{
    /* Each glyph is 7 rows, 5 bits wide, MSB-first in the low 5 bits (bit 4..0). */
    static const uint8_t SPACE[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t A[7] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    static const uint8_t D[7] = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
    static const uint8_t E[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
    static const uint8_t N[7] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
    static const uint8_t S[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    static const uint8_t U[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};

    switch (c)
    {
        case 'A': return A;
        case 'D': return D;
        case 'E': return E;
        case 'N': return N;
        case 'S': return S;
        case 'U': return U;
        case ' ': return SPACE;
        default: return SPACE;
    }
}

static void edgeai_draw_char5x7_scaled(int32_t x, int32_t y, int32_t scale, char c, uint16_t color)
{
    const uint8_t *g = edgeai_glyph5x7(c);
    if (scale < 1) scale = 1;

    for (int32_t row = 0; row < 7; row++)
    {
        uint8_t bits = g[row];
        for (int32_t col = 0; col < 5; col++)
        {
            if (bits & (1u << (4 - col)))
            {
                int32_t x0 = x + col * scale;
                int32_t y0 = y + row * scale;
                par_lcd_s035_fill_rect(x0, y0, x0 + scale - 1, y0 + scale - 1, color);
            }
        }
    }
}

static void edgeai_draw_text5x7_scaled(int32_t x, int32_t y, int32_t scale, const char *s, uint16_t color)
{
    int32_t cx = x;
    while (*s)
    {
        edgeai_draw_char5x7_scaled(cx, y, scale, *s, color);
        cx += (5 + 1) * scale;
        s++;
    }
}

static int32_t edgeai_text5x7_w(int32_t scale, const char *s)
{
    int32_t n = 0;
    while (s[n]) n++;
    if (n == 0) return 0;
    return n * (5 + 1) * scale - 1 * scale; /* last char doesn't need trailing space */
}

static void edgeai_draw_boot_title_sand_dune(void)
{
    const int32_t scale = 7;
    const char *l1 = "SAND";
    const char *l2 = "DUNE";

    int32_t w1 = edgeai_text5x7_w(scale, l1);
    int32_t w2 = edgeai_text5x7_w(scale, l2);
    int32_t w = (w1 > w2) ? w1 : w2;
    int32_t h = 2 * (7 * scale) + (2 * scale);

    int32_t x = (LCD_W - w) / 2;
    int32_t y = (LCD_H - h) / 2;

    /* Simple 3D look: shadow then face. */
    const int32_t ox = 4;
    const int32_t oy = 4;
    const uint16_t shadow = 0x2104u; /* dark gray */
    const uint16_t face = 0xFFDEu;   /* warm white */

    edgeai_draw_text5x7_scaled(x + ox, y + oy, scale, l1, shadow);
    edgeai_draw_text5x7_scaled(x + ox, y + oy + (7 * scale) + (2 * scale), scale, l2, shadow);
    edgeai_draw_text5x7_scaled(x, y, scale, l1, face);
    edgeai_draw_text5x7_scaled(x, y + (7 * scale) + (2 * scale), scale, l2, face);
}

int main(void)
{
    BOARD_InitHardware();
    dwt_cycle_counter_init();

    /* Bring up LCD early so the demo remains visibly alive even if accel init fails. */
    if (!par_lcd_s035_init())
    {
        /* Can't proceed without display in this demo. */
        for (;;) {}
    }
    par_lcd_s035_fill(0x0000u); /* boot stays black (dune reveals with motion) */
    edgeai_draw_boot_title_sand_dune();
    SDK_DelayAtLeastUs(3000000u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    par_lcd_s035_fill(0x0000u);

    /* Print banner early; previous hangs made it hard to tell if firmware was alive. */
    PRINTF("EDGEAI: boot %s %s\r\n", __DATE__, __TIME__);

    /* Init I2C for the accel (mikroBUS). */
    lpi2c_master_config_t masterCfg;
    LPI2C_MasterGetDefaultConfig(&masterCfg);
    /* Be conservative; some shield/cable setups are flaky at 400k. */
    masterCfg.baudRate_Hz = 100000u;
    LPI2C_MasterInit(EDGEAI_I2C, &masterCfg, edgeai_i2c_get_freq());

    fxls8974_dev_t dev = {
        .addr7 = 0,
        .write = edgeai_i2c_write,
        .read = edgeai_i2c_read,
    };

    const uint8_t addrs[] = {ACCEL4_CLICK_I2C_ADDR0, ACCEL4_CLICK_I2C_ADDR1};
    uint8_t who = 0;
    bool found = false;
    /* Retry accel bring-up briefly; cold boots can race sensor power-up. */
    for (uint32_t tries = 0; tries < 200; tries++) /* ~2s */
    {
        for (size_t i = 0; i < (sizeof(addrs) / sizeof(addrs[0])); i++)
        {
            dev.addr7 = addrs[i];
            if (fxls8974_read_whoami(&dev, &who) && (who == FXLS8974_WHO_AM_I_VALUE))
            {
                found = true;
                break;
            }
        }
        if (found) break;
        SDK_DelayAtLeastUs(10000u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    }
    if (!found)
    {
        PRINTF("EDGEAI: FXLS8974CF not found (WHO_AM_I=0x%02x). Continuing without accel.\r\n", who);
    }
    else
    {
        (void)fxls8974_set_active(&dev, false);
        (void)fxls8974_set_fsr(&dev, FXLS8974_FSR_4G);
        (void)fxls8974_set_active(&dev, true);
        PRINTF("EDGEAI: accel ok addr=0x%02x\r\n", (unsigned)dev.addr7);
    }

    bool npu_ok = false;
    edgeai_tensor_dims_t in_dims = {0};
    edgeai_tensor_type_t in_type = kEdgeAiTensorType_UINT8;
    uint8_t *in_data = NULL;
    edgeai_tensor_dims_t out_dims = {0};
    edgeai_tensor_type_t out_type = kEdgeAiTensorType_UINT8;
    uint8_t *out_data = NULL;
    /* Guard NPU init behind the inference flag to keep boot stable on setups where
     * model init may fault or stall.
     */
    if (EDGEAI_ENABLE_NPU_INFERENCE)
    {
        npu_ok = (EDGEAI_MODEL_Init() == kStatus_Success);
    }
    if (npu_ok)
    {
        in_data = EDGEAI_MODEL_GetInputTensorData(&in_dims, &in_type);
        out_data = EDGEAI_MODEL_GetOutputTensorData(&out_dims, &out_type);
        npu_ok = (in_data != NULL) && (out_data != NULL);
    }

    /* Ball state (Q16.16 pixels). */
    int32_t x_q16 = (LCD_W / 2) << 16;
    int32_t y_q16 = (LCD_H / 2) << 16;
    int32_t vx_q16 = 0;
    int32_t vy_q16 = 0;
    int32_t prev_x = LCD_W / 2;
    int32_t prev_y = LCD_H / 2;

    /* Simple trail. */
    enum { TRAIL_N = 12 };
    int16_t trail_x[TRAIL_N];
    int16_t trail_y[TRAIL_N];
    for (int i = 0; i < TRAIL_N; i++) { trail_x[i] = (int16_t)prev_x; trail_y[i] = (int16_t)prev_y; }
    uint32_t trail_head = 0;

    /* Simple low-pass on accel; map tilt directly to screen position. */
    int32_t ax_lp = 0;
    int32_t ay_lp = 0;

    fxls8974_sample_t s = {0};
    uint32_t last_cyc = DWT->CYCCNT;
    uint32_t frame = 0;
    uint8_t glint = 0;
    uint32_t accel_fail = 0;
    uint32_t npu_accum_us = 0;
    uint32_t stats_accum_us = 0;
    uint32_t stats_frames = 0;
    uint32_t fps_last = 0;

    /* Maximum dirty-rect size. Even in raster mode, clamp work per frame. */
    enum { TILE_MAX_W = 200, TILE_MAX_H = 200 };
#if EDGEAI_RENDER_SINGLE_BLIT
    /* Tile renderer (one LCD blit per frame to avoid tearing/flicker). */
    static uint16_t tile[TILE_MAX_W * TILE_MAX_H];
#endif

    /* Boot banner: keep it short and printf-lite compatible (avoid %ld). */
    PRINTF("EDGEAI: tilt-ball (npu_init=%u npu_run=%u render=%s)\r\n",
           (unsigned)(npu_ok ? 1u : 0u),
           (unsigned)(EDGEAI_ENABLE_NPU_INFERENCE ? 1u : 0u),
           (EDGEAI_RENDER_SINGLE_BLIT ? "blit" : "raster"));

    /* Some MCXN947 configurations (or secure setups) can leave DWT->CYCCNT not advancing,
     * which makes dt==0 and "freezes" all motion. Keep a fixed timestep fallback so
     * the demo always behaves predictably.
     */
    const int32_t dt_fallback_q16 = (int32_t)((1u << 16) / 60u); /* ~16.67ms */
    const uint32_t dt_fallback_us = 16667u;

    /* Fixed-step simulation + throttled rendering.
     * Without this, if the loop runs very fast (small dt), the per-iteration damping can
     * effectively "lock" velocity near zero and the ball looks stuck.
     */
    const int32_t sim_step_q16 = (int32_t)((1u << 16) / 120u); /* 120 Hz physics */
    int32_t sim_accum_q16 = 0;
    uint32_t render_accum_us = 0;
    const uint32_t render_period_us = 16667u; /* ~60 FPS */

    for (;;)
    {
        bool accel_ok = false;
        if (found)
        {
            accel_ok = fxls8974_read_sample_12b(&dev, &s);
        }
        if (!accel_ok)
        {
            /* Keep the render loop alive even if I2C glitches; this prevents the
             * display from appearing "frozen" without any indication why.
             */
            accel_fail++;
            s.x = 0;
            s.y = 0;
            s.z = 0;
        }
        else
        {
            accel_fail = 0;
        }

        uint32_t now = DWT->CYCCNT;
        uint32_t dc = now - last_cyc;
        last_cyc = now;
        uint32_t cps = SystemCoreClock ? SystemCoreClock : 150000000u;

        /* dt in Q16 seconds */
        int32_t dt_q16 = dt_fallback_q16;
        uint32_t dt_us = dt_fallback_us;
        if ((dc != 0u) && (cps != 0u))
        {
            dt_q16 = (int32_t)(((uint64_t)dc << 16) / (uint64_t)cps);
            dt_us = (uint32_t)(((uint64_t)dc * 1000000u) / (uint64_t)cps);
            if (dt_q16 <= 0) dt_q16 = dt_fallback_q16;
            if (dt_us == 0u) dt_us = dt_fallback_us;
        }

        if (dt_q16 > (int32_t)(1u << 16)) dt_q16 = (int32_t)(1u << 16);
        npu_accum_us += dt_us;
        stats_accum_us += dt_us;
        render_accum_us += dt_us;
        sim_accum_q16 += dt_q16;

        int32_t ax = (int32_t)s.x;
        int32_t ay = (int32_t)s.y;
        map_accel_xy(&ax, &ay);

        /* Faster LP so small tilts respond quickly. */
        ax_lp += (ax - ax_lp) >> 2;
        ay_lp += (ay - ay_lp) >> 2;

        /* Raw 12b is roughly ~512 counts / 1g at the current mode. Clamp to avoid crazy jumps. */
        ax_lp = clamp_i32(ax_lp, -ACCEL_MAP_DENOM * 2, ACCEL_MAP_DENOM * 2);
        ay_lp = clamp_i32(ay_lp, -ACCEL_MAP_DENOM * 2, ACCEL_MAP_DENOM * 2);

        /* Deadzone + soft response curve (arcade "marble" feel).
         * We use a small deadzone then a blended linear/cubic curve so it isn't twitchy
         * near center, but still responds strongly at larger tilts.
         */
        const int32_t deadzone = 10; /* counts */
        int32_t ax_dz = ax_lp;
        int32_t ay_dz = ay_lp;
        if (abs_i32(ax_dz) <= deadzone) ax_dz = 0;
        else ax_dz -= (ax_dz > 0) ? deadzone : -deadzone;
        if (abs_i32(ay_dz) <= deadzone) ay_dz = 0;
        else ay_dz -= (ay_dz > 0) ? deadzone : -deadzone;

        /* Normalize to Q15 ~= [-1,1] at 1g. */
        int32_t ax_n_q15 = (int32_t)(((int64_t)ax_dz << 15) / (int64_t)ACCEL_MAP_DENOM);
        int32_t ay_n_q15 = (int32_t)(((int64_t)ay_dz << 15) / (int64_t)ACCEL_MAP_DENOM);
        ax_n_q15 = clamp_i32_sym(ax_n_q15, 32767);
        ay_n_q15 = clamp_i32_sym(ay_n_q15, 32767);

        /* cubic = x^3 (Q15) */
        int32_t ax2 = (int32_t)(((int64_t)ax_n_q15 * ax_n_q15) >> 15);
        int32_t ay2 = (int32_t)(((int64_t)ay_n_q15 * ay_n_q15) >> 15);
        int32_t ax_cu_q15 = (int32_t)(((int64_t)ax2 * ax_n_q15) >> 15);
        int32_t ay_cu_q15 = (int32_t)(((int64_t)ay2 * ay_n_q15) >> 15);

        /* Blend: 35% linear + 65% cubic (alpha in Q15). */
        const int32_t alpha_q15 = 11469; /* ~0.35 */
        int32_t ax_soft_q15 = (int32_t)(((int64_t)ax_n_q15 * alpha_q15 +
                                         (int64_t)ax_cu_q15 * ((1 << 15) - alpha_q15)) >> 15);
        int32_t ay_soft_q15 = (int32_t)(((int64_t)ay_n_q15 * alpha_q15 +
                                         (int64_t)ay_cu_q15 * ((1 << 15) - alpha_q15)) >> 15);

        /* If accel read is failing, force a visible change so it doesn't look like
         * the demo is broken in a mysterious way.
         */
#if !EDGEAI_RENDER_SINGLE_BLIT
        uint16_t bg = (accel_fail > 0) ? 0x1800u /* dark red */ : 0x0000u;
#endif

        /* Physics: fixed-step integration at sim_step_q16. */
        const int32_t a_px_s2 = 4200; /* px/s^2 at ~1g (arcade, not twitchy) */
        /* soft_q15 * a_px_s2 gives Q15 px/s^2; convert to Q16 by <<1 */
        int32_t ax_a_q16 = (int32_t)(((int64_t)ax_soft_q15 * a_px_s2) << 1);
        int32_t ay_a_q16 = (int32_t)(((int64_t)ay_soft_q15 * a_px_s2) << 1);

        /* Use the max radius here so the "near" ball can't clip the screen edge. */
        const int32_t minx = BALL_R_MAX + 2;
        const int32_t miny = BALL_R_MAX + 2;
        const int32_t maxx = (LCD_W - 1) - (BALL_R_MAX + 2);
        const int32_t maxy = (LCD_H - 1) - (BALL_R_MAX + 2);

        int iter = 0;
        while ((sim_accum_q16 >= sim_step_q16) && (iter < 6))
        {
            sim_accum_q16 -= sim_step_q16;
            iter++;

            vx_q16 += (int32_t)(((int64_t)ax_a_q16 * sim_step_q16) >> 16);
            vy_q16 += (int32_t)(((int64_t)ay_a_q16 * sim_step_q16) >> 16);

            /* Damping (Q16) tuned for the fixed sim rate. */
            const int32_t damp = 65000; /* ~0.992 @ 120 Hz */
            vx_q16 = (int32_t)(((int64_t)vx_q16 * damp) >> 16);
            vy_q16 = (int32_t)(((int64_t)vy_q16 * damp) >> 16);

            x_q16 += (int32_t)(((int64_t)vx_q16 * sim_step_q16) >> 16);
            y_q16 += (int32_t)(((int64_t)vy_q16 * sim_step_q16) >> 16);

            int32_t cx_s = x_q16 >> 16;
            int32_t cy_s = y_q16 >> 16;
            if (cx_s < minx) { cx_s = minx; x_q16 = cx_s << 16; vx_q16 = -(vx_q16 * 3) / 4; }
            if (cx_s > maxx) { cx_s = maxx; x_q16 = cx_s << 16; vx_q16 = -(vx_q16 * 3) / 4; }
            if (cy_s < miny) { cy_s = miny; y_q16 = cy_s << 16; vy_q16 = -(vy_q16 * 3) / 4; }
            if (cy_s > maxy) { cy_s = maxy; y_q16 = cy_s << 16; vy_q16 = -(vy_q16 * 3) / 4; }
        }

        int32_t cx = x_q16 >> 16;
        int32_t cy = y_q16 >> 16;
        int32_t r_draw = edgeai_ball_r_for_y(cy);

        bool do_render = (render_accum_us >= render_period_us);
        if (do_render) render_accum_us = 0;

        if (do_render)
        {
            /* The trail is a ring buffer; one point is "removed" each frame when overwriting
             * the head slot. Include that removed point in the dirty rect so it is cleared,
             * otherwise stale dots can remain on-screen.
             */
            int32_t removed_tx = trail_x[trail_head];
            int32_t removed_ty = trail_y[trail_head];

            /* Update trail ring. */
            trail_x[trail_head] = (int16_t)cx;
            trail_y[trail_head] = (int16_t)cy;
            trail_head = (trail_head + 1u) % TRAIL_N;

            /* Dirty rect: cover previous and current ball + trail area. */
            int32_t minx_r = cx, miny_r = cy, maxx_r = cx, maxy_r = cy;
            for (int i = 0; i < TRAIL_N; i++)
            {
                int32_t tx = trail_x[i];
                int32_t ty = trail_y[i];
                if (tx < minx_r) minx_r = tx;
                if (ty < miny_r) miny_r = ty;
                if (tx > maxx_r) maxx_r = tx;
                if (ty > maxy_r) maxy_r = ty;
            }
            if (removed_tx < minx_r) minx_r = removed_tx;
            if (removed_ty < miny_r) miny_r = removed_ty;
            if (removed_tx > maxx_r) maxx_r = removed_tx;
            if (removed_ty > maxy_r) maxy_r = removed_ty;
            if (prev_x < minx_r) minx_r = prev_x;
            if (prev_y < miny_r) miny_r = prev_y;
            if (prev_x > maxx_r) maxx_r = prev_x;
            if (prev_y > maxy_r) maxy_r = prev_y;

            int32_t pad = BALL_R_MAX + 30;
            int32_t x0 = clamp_i32(minx_r - pad, 0, LCD_W - 1);
            int32_t y0 = clamp_i32(miny_r - pad, 0, LCD_H - 1);
            int32_t x1 = clamp_i32(maxx_r + pad, 0, LCD_W - 1);
            int32_t y1 = clamp_i32(maxy_r + pad, 0, LCD_H - 1);

            /* Status overlay region (top-right). Force redraw of this small rect. */
            const int32_t ov_w = 96;
            const int32_t ov_h = 9;
            const int32_t ov_x0 = LCD_W - ov_w - 2;
            const int32_t ov_y0 = 2;
            const int32_t ov_x1 = LCD_W - 2;
            const int32_t ov_y1 = ov_y0 + ov_h - 1;
            if (ov_x0 < x0) x0 = ov_x0;
            if (ov_y0 < y0) y0 = ov_y0;
            if (ov_x1 > x1) x1 = ov_x1;
            if (ov_y1 > y1) y1 = ov_y1;

            /* Clamp tile size; if motion ever causes a large dirty rect, cap it to a fixed size. */
            int32_t w = x1 - x0 + 1;
            int32_t h = y1 - y0 + 1;
            if (w > TILE_MAX_W || h > TILE_MAX_H)
            {
                int32_t halfw = TILE_MAX_W / 2;
                int32_t halfh = TILE_MAX_H / 2;
                int32_t ccx = (minx_r + maxx_r) / 2;
                int32_t ccy = (miny_r + maxy_r) / 2;
                x0 = clamp_i32(ccx - halfw, 0, LCD_W - 1);
                y0 = clamp_i32(ccy - halfh, 0, LCD_H - 1);
                x1 = clamp_i32(x0 + TILE_MAX_W - 1, 0, LCD_W - 1);
                y1 = clamp_i32(y0 + TILE_MAX_H - 1, 0, LCD_H - 1);
                w = x1 - x0 + 1;
                h = y1 - y0 + 1;
            }

#if EDGEAI_RENDER_SINGLE_BLIT
            /* Render into tile (black background) then blit once to avoid flicker/tearing lines. */
            sw_render_dune_bg(tile, (uint32_t)w, (uint32_t)h, x0, y0);

            /* Trails (streaks). */
            for (int i = 0; i < TRAIL_N; i++)
            {
                uint32_t idx = (trail_head + (uint32_t)i) % TRAIL_N;
                int32_t tx = trail_x[idx];
                int32_t ty = trail_y[idx];
                int r0 = 1 + (i / 6);
                uint16_t c = (i < 6) ? 0x39E7u : 0x18C3u;
                sw_render_filled_circle(tile, (uint32_t)w, (uint32_t)h, x0, y0, tx, ty, r0, c);
            }

            /* Depth cue: scale the ball/shadow with y-position. */
            sw_render_ball_shadow(tile, (uint32_t)w, (uint32_t)h, x0, y0, cx, cy, r_draw);
            sw_render_silver_ball(tile, (uint32_t)w, (uint32_t)h, x0, y0, cx, cy, r_draw, frame++, glint);

            /* Tiny status text in upper-right (blue). */
            char d3[4];
            edgeai_u32_to_dec3(d3, fps_last);
            char status[32];
            /* Format: C:XYZ N:0 I:0 (digits fixed-width) */
            status[0] = 'C'; status[1] = ':'; status[2] = d3[0]; status[3] = d3[1]; status[4] = d3[2];
            status[5] = ' '; status[6] = 'N'; status[7] = ':'; status[8] = npu_ok ? '1' : '0';
            status[9] = ' '; status[10] = 'I'; status[11] = ':'; status[12] = EDGEAI_ENABLE_NPU_INFERENCE ? '1' : '0';
            status[13] = '\0';
            sw_render_text5x7(tile, (uint32_t)w, (uint32_t)h, x0, y0, ov_x0, ov_y0, status, 0x001Fu);

            par_lcd_s035_blit_rect(x0, y0, x1, y1, tile);
#else
            /* "Raster" mode: draw directly to LCD using multiple operations.
             * This is intentionally not a single-blit path; it can show tearing/raster lines.
             */
            par_lcd_s035_fill_rect(x0, y0, x1, y1, bg);

            for (int i = 0; i < TRAIL_N; i++)
            {
                uint32_t idx = (trail_head + (uint32_t)i) % TRAIL_N;
                int32_t tx = trail_x[idx];
                int32_t ty = trail_y[idx];
                int r0 = 1 + (i / 6);
                uint16_t c = (i < 6) ? 0x39E7u : 0x18C3u;
                par_lcd_s035_draw_filled_circle(tx, ty, r0, c);
            }

            /* Depth cue: scale the ball/shadow with y-position. */
            par_lcd_s035_draw_ball_shadow(cx, cy, r_draw);
            par_lcd_s035_draw_silver_ball(cx, cy, r_draw, frame++, glint);

            /* Status text (blue) on a small black backing box. */
            char d3[4];
            edgeai_u32_to_dec3(d3, fps_last);
            char status[32];
            status[0] = 'C'; status[1] = ':'; status[2] = d3[0]; status[3] = d3[1]; status[4] = d3[2];
            status[5] = ' '; status[6] = 'N'; status[7] = ':'; status[8] = npu_ok ? '1' : '0';
            status[9] = ' '; status[10] = 'I'; status[11] = ':'; status[12] = EDGEAI_ENABLE_NPU_INFERENCE ? '1' : '0';
            status[13] = '\0';
            par_lcd_s035_fill_rect(ov_x0, ov_y0, ov_x1, ov_y1, 0x0000u);
            edgeai_draw_text5x7_scaled(ov_x0, ov_y0, 1, status, 0x001Fu);
#endif

            prev_x = cx;
            prev_y = cy;
            stats_frames++;
        }

        /* NPU hook: run a Neutron-backed TFLM model at a low rate and use output to modulate "glint". */
        if (EDGEAI_ENABLE_NPU_INFERENCE && npu_ok && (npu_accum_us >= 200000u))
        {
            npu_accum_us = 0;

            /* Fill input tensor with a synthetic pattern derived from motion/tilt.
             * This is a placeholder input; replace with real features later.
             */
            uint32_t n = 1;
            if (in_dims.size >= 2) n = in_dims.data[1];
            if (in_dims.size >= 3) n *= in_dims.data[2];
            if (in_dims.size >= 4) n *= in_dims.data[3];

            uint8_t base = 127u;
            uint8_t amp = (uint8_t)clamp_i32((int32_t)((vx_q16 < 0 ? -vx_q16 : vx_q16) >> 16) +
                                             (int32_t)((vy_q16 < 0 ? -vy_q16 : vy_q16) >> 16), 0, 80);
            for (uint32_t i = 0; i < n; i++)
            {
                /* simple repeating waveform */
                uint8_t t = (uint8_t)(i & 31u);
                uint8_t v = (t < 16u) ? (uint8_t)(base + (amp * t) / 16u) : (uint8_t)(base + (amp * (31u - t)) / 16u);
                in_data[i] = v;
            }

            if (EDGEAI_MODEL_RunInference() == kStatus_Success)
            {
                /* Convert model output to 0..255. */
                uint32_t m = 0;
                uint32_t out_n = 1;
                if (out_dims.size >= 1) out_n = out_dims.data[0];
                if (out_dims.size >= 2) out_n *= out_dims.data[1];
                if (out_dims.size >= 3) out_n *= out_dims.data[2];
                if (out_dims.size >= 4) out_n *= out_dims.data[3];
                if (out_n == 0) out_n = 1;

                if (out_type == kEdgeAiTensorType_UINT8)
                {
                    for (uint32_t i = 0; i < out_n; i++) if (out_data[i] > m) m = out_data[i];
                    glint = (uint8_t)m;
                }
                else if (out_type == kEdgeAiTensorType_INT8)
                {
                    int8_t *p = (int8_t *)out_data;
                    int32_t mm = -128;
                    for (uint32_t i = 0; i < out_n; i++) if ((int32_t)p[i] > mm) mm = (int32_t)p[i];
                    glint = (uint8_t)clamp_i32(mm + 128, 0, 255);
                }
                else
                {
                    /* float32 */
                    float *p = (float *)out_data;
                    float mm = p[0];
                    for (uint32_t i = 1; i < out_n; i++) if (p[i] > mm) mm = p[i];
                    if (mm < 0.0f) mm = 0.0f;
                    if (mm > 1.0f) mm = 1.0f;
                    glint = (uint8_t)(mm * 255.0f);
                }
            }
        }

        if (stats_accum_us >= 1000000u)
        {
            uint32_t fps = stats_frames;
            fps_last = fps;
            stats_accum_us = 0;
            stats_frames = 0;
            PRINTF("EDGEAI: fps=%u raw=(%d,%d,%d) lp=(%d,%d) pos=(%d,%d) v=(%d,%d) glint=%u npu=%u\r\n",
                   (unsigned)fps,
                   (int)s.x, (int)s.y, (int)s.z,
                   (int)ax_lp, (int)ay_lp,
                   (int)cx, (int)cy,
                   (int)(vx_q16 >> 16), (int)(vy_q16 >> 16),
                   (unsigned)glint,
                   (unsigned)(npu_ok ? 1u : 0u));
        }
    }
}
