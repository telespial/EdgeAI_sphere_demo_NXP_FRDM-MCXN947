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

#define BALL_R 20

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

/* Rendering mode:
 * - 0: "raster/flicker" mode: draw primitives directly to LCD using many small writes
 *      (visually interesting but can show tearing/shutter lines).
 * - 1: "single blit" mode: render into a RAM tile and blit once per frame
 *      (stable image, much less tearing).
 */
#ifndef EDGEAI_RENDER_SINGLE_BLIT
#define EDGEAI_RENDER_SINGLE_BLIT 0
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

int main(void)
{
    BOARD_InitHardware();
    dwt_cycle_counter_init();

    /* Init I2C for the accel (mikroBUS). */
    lpi2c_master_config_t masterCfg;
    LPI2C_MasterGetDefaultConfig(&masterCfg);
    masterCfg.baudRate_Hz = 400000u;
    LPI2C_MasterInit(EDGEAI_I2C, &masterCfg, edgeai_i2c_get_freq());

    fxls8974_dev_t dev = {
        .addr7 = 0,
        .write = edgeai_i2c_write,
        .read = edgeai_i2c_read,
    };

    const uint8_t addrs[] = {ACCEL4_CLICK_I2C_ADDR0, ACCEL4_CLICK_I2C_ADDR1};
    uint8_t who = 0;
    bool found = false;
    for (size_t i = 0; i < (sizeof(addrs) / sizeof(addrs[0])); i++)
    {
        dev.addr7 = addrs[i];
        if (fxls8974_read_whoami(&dev, &who) && (who == FXLS8974_WHO_AM_I_VALUE))
        {
            found = true;
            break;
        }
    }
    if (!found)
    {
        PRINTF("FXLS8974CF not found (WHO_AM_I=0x%02x)\r\n", who);
        for (;;) {}
    }
    (void)fxls8974_set_active(&dev, false);
    (void)fxls8974_set_fsr(&dev, FXLS8974_FSR_4G);
    (void)fxls8974_set_active(&dev, true);

    if (!par_lcd_s035_init())
    {
        PRINTF("LCD init failed\r\n");
        for (;;) {}
    }

    par_lcd_s035_fill(0x0000u);

    bool npu_ok = (EDGEAI_MODEL_Init() == kStatus_Success);
    edgeai_tensor_dims_t in_dims = {0};
    edgeai_tensor_type_t in_type = kEdgeAiTensorType_UINT8;
    uint8_t *in_data = NULL;
    edgeai_tensor_dims_t out_dims = {0};
    edgeai_tensor_type_t out_type = kEdgeAiTensorType_UINT8;
    uint8_t *out_data = NULL;
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

    /* Simple low-pass on accel; we map tilt directly to screen position. */
    int32_t ax_lp = 0;
    int32_t ay_lp = 0;

    fxls8974_sample_t s = {0};
    uint32_t last_cyc = DWT->CYCCNT;
    uint32_t frame = 0;
    uint8_t glint = 0;
    uint32_t npu_accum_us = 0;
    uint32_t stats_accum_us = 0;
    uint32_t stats_frames = 0;

    /* Maximum dirty-rect size. Even in raster mode we clamp work per frame. */
    enum { TILE_MAX_W = 200, TILE_MAX_H = 200 };
#if EDGEAI_RENDER_SINGLE_BLIT
    /* Tile renderer (one LCD blit per frame to avoid tearing/flicker). */
    static uint16_t tile[TILE_MAX_W * TILE_MAX_H];
#endif

    /* Some MCXN947 configurations (or secure setups) can leave DWT->CYCCNT not advancing,
     * which makes dt==0 and "freezes" all motion. Keep a fixed timestep fallback so
     * the demo always behaves predictably.
     */
    const int32_t dt_fallback_q16 = (int32_t)((1u << 16) / 60u); /* ~16.67ms */
    const uint32_t dt_fallback_us = 16667u;

    for (;;)
    {
        if (!fxls8974_read_sample_12b(&dev, &s))
        {
            continue;
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

        int32_t ax = (int32_t)s.x;
        int32_t ay = (int32_t)s.y;
        map_accel_xy(&ax, &ay);

        ax_lp += (ax - ax_lp) >> 3;
        ay_lp += (ay - ay_lp) >> 3;

        /* Raw 12b is roughly ~512 counts / 1g at our current mode. Clamp to avoid crazy jumps. */
        ax_lp = clamp_i32(ax_lp, -ACCEL_MAP_DENOM * 2, ACCEL_MAP_DENOM * 2);
        ay_lp = clamp_i32(ay_lp, -ACCEL_MAP_DENOM * 2, ACCEL_MAP_DENOM * 2);

        /* Deadzone to avoid jitter when the board is nearly flat. */
        if (abs_i32(ax_lp) < 6) ax_lp = 0;
        if (abs_i32(ay_lp) < 6) ay_lp = 0;

        /* Physics: treat accel as "gravity" and integrate velocity/position. */
        const int32_t a_px_s2 = 2400; /* px/s^2 at ~1g (more responsive) */
        const int32_t a_scale_q16 = (int32_t)(((int64_t)a_px_s2 << 16) / ACCEL_MAP_DENOM);
        int32_t ax_a_q16 = ax_lp * a_scale_q16;
        int32_t ay_a_q16 = ay_lp * a_scale_q16;

        vx_q16 += (int32_t)(((int64_t)ax_a_q16 * dt_q16) >> 16);
        vy_q16 += (int32_t)(((int64_t)ay_a_q16 * dt_q16) >> 16);

        /* Damping (Q16). */
        const int32_t damp = 64200; /* ~0.98 */
        vx_q16 = (int32_t)(((int64_t)vx_q16 * damp) >> 16);
        vy_q16 = (int32_t)(((int64_t)vy_q16 * damp) >> 16);

        x_q16 += (int32_t)(((int64_t)vx_q16 * dt_q16) >> 16);
        y_q16 += (int32_t)(((int64_t)vy_q16 * dt_q16) >> 16);

        /* Bounds + bounce. */
        const int32_t minx = BALL_R + 2;
        const int32_t miny = BALL_R + 2;
        const int32_t maxx = (LCD_W - 1) - (BALL_R + 2);
        const int32_t maxy = (LCD_H - 1) - (BALL_R + 2);
        int32_t cx = x_q16 >> 16;
        int32_t cy = y_q16 >> 16;
        if (cx < minx) { cx = minx; x_q16 = cx << 16; vx_q16 = -(vx_q16 * 3) / 4; }
        if (cx > maxx) { cx = maxx; x_q16 = cx << 16; vx_q16 = -(vx_q16 * 3) / 4; }
        if (cy < miny) { cy = miny; y_q16 = cy << 16; vy_q16 = -(vy_q16 * 3) / 4; }
        if (cy > maxy) { cy = maxy; y_q16 = cy << 16; vy_q16 = -(vy_q16 * 3) / 4; }

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
        if (prev_x < minx_r) minx_r = prev_x;
        if (prev_y < miny_r) miny_r = prev_y;
        if (prev_x > maxx_r) maxx_r = prev_x;
        if (prev_y > maxy_r) maxy_r = prev_y;

        int32_t pad = BALL_R + 30;
        int32_t x0 = clamp_i32(minx_r - pad, 0, LCD_W - 1);
        int32_t y0 = clamp_i32(miny_r - pad, 0, LCD_H - 1);
        int32_t x1 = clamp_i32(maxx_r + pad, 0, LCD_W - 1);
        int32_t y1 = clamp_i32(maxy_r + pad, 0, LCD_H - 1);

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
        sw_render_clear(tile, (uint32_t)w, (uint32_t)h, 0x0000u);

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

        sw_render_ball_shadow(tile, (uint32_t)w, (uint32_t)h, x0, y0, cx, cy, BALL_R);
        sw_render_silver_ball(tile, (uint32_t)w, (uint32_t)h, x0, y0, cx, cy, BALL_R, frame++, glint);

        par_lcd_s035_blit_rect(x0, y0, x1, y1, tile);
#else
        /* "Raster" mode: draw directly to LCD using multiple operations.
         * This is intentionally not a single-blit path; it can show tearing/raster lines.
         */
        par_lcd_s035_fill_rect(x0, y0, x1, y1, 0x0000u);

        for (int i = 0; i < TRAIL_N; i++)
        {
            uint32_t idx = (trail_head + (uint32_t)i) % TRAIL_N;
            int32_t tx = trail_x[idx];
            int32_t ty = trail_y[idx];
            int r0 = 1 + (i / 6);
            uint16_t c = (i < 6) ? 0x39E7u : 0x18C3u;
            par_lcd_s035_draw_filled_circle(tx, ty, r0, c);
        }

        par_lcd_s035_draw_ball_shadow(cx, cy, BALL_R);
        par_lcd_s035_draw_silver_ball(cx, cy, BALL_R, frame++, glint);
#endif

        prev_x = cx;
        prev_y = cy;
        stats_frames++;

        /* NPU hook: run a Neutron-backed TFLM model at a low rate and use output to modulate "glint". */
        if (npu_ok && (npu_accum_us >= 200000u))
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
            stats_accum_us = 0;
            stats_frames = 0;
            PRINTF("fps=%u ax=%ld ay=%ld cx=%ld cy=%ld vx=%ld vy=%ld glint=%u npu=%u\r\n",
                   (unsigned)fps,
                   (long)ax_lp, (long)ay_lp,
                   (long)cx, (long)cy,
                   (long)(vx_q16 >> 16), (long)(vy_q16 >> 16),
                   (unsigned)glint,
                   (unsigned)(npu_ok ? 1u : 0u));
        }
    }
}
