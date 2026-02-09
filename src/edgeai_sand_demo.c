#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "accel4_click.h"
#include "accel_proc.h"
#include "edgeai_config.h"
#include "edgeai_util.h"
#include "fxls8974cf.h"
#include "npu_api.h"
#include "par_lcd_s035.h"
#include "render_world.h"
#include "sim_world.h"
#include "text5x7.h"

#include "app.h"
#include "board.h"
#include "clock_config.h"
#include "fsl_clock.h"
#include "fsl_debug_console.h"
#include "fsl_lpi2c.h"
#include "pin_mux.h"

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

static void dwt_cycle_counter_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void edgeai_draw_boot_title_sand_dune(void)
{
    const int32_t scale = 7;
    const char *l1 = "SAND";
    const char *l2 = "DUNE";

    int32_t w1 = edgeai_text5x7_width(scale, l1);
    int32_t w2 = edgeai_text5x7_width(scale, l2);
    int32_t w = (w1 > w2) ? w1 : w2;
    int32_t h = 2 * (7 * scale) + (2 * scale);

    int32_t x = (EDGEAI_LCD_W - w) / 2;
    int32_t y = (EDGEAI_LCD_H - h) / 2;

    /* Simple 3D look: shadow then face. */
    const int32_t ox = 4;
    const int32_t oy = 4;
    const uint16_t shadow = 0x2104u; /* dark gray */
    const uint16_t face = 0xFFDEu;   /* warm white */

    edgeai_text5x7_draw_scaled(x + ox, y + oy, scale, l1, shadow);
    edgeai_text5x7_draw_scaled(x + ox, y + oy + (7 * scale) + (2 * scale), scale, l2, shadow);
    edgeai_text5x7_draw_scaled(x, y, scale, l1, face);
    edgeai_text5x7_draw_scaled(x, y + (7 * scale) + (2 * scale), scale, l2, face);
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
    par_lcd_s035_fill(0x0000u); /* black behind boot title */
    edgeai_draw_boot_title_sand_dune();
    SDK_DelayAtLeastUs(3000000u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    render_world_draw_full_background();

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

    edgeai_npu_state_t npu = {0};
    bool npu_ok = edgeai_npu_init(&npu);

    accel_proc_t accel_proc;
    accel_proc_init(&accel_proc);

    sim_world_t world;
    sim_world_init(&world, EDGEAI_LCD_W, EDGEAI_LCD_H);

    render_state_t rs;
    render_world_init(&rs, EDGEAI_LCD_W / 2, EDGEAI_LCD_H / 2);

    fxls8974_sample_t s = {0};
    uint32_t last_cyc = DWT->CYCCNT;
    uint32_t accel_fail = 0;
    uint32_t npu_accum_us = 0;
    uint32_t stats_accum_us = 0;
    uint32_t stats_frames = 0;
    uint32_t fps_last = 0;

    /* Boot banner: keep it short and printf-lite compatible (avoid %ld). */
    PRINTF("EDGEAI: tilt-ball (npu_backend=%c npu_init=%u npu_run=%u render=%s)\r\n",
           edgeai_npu_backend_char(),
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

    sim_params_t sim_p;
    sim_p.sim_step_q16 = sim_step_q16;
    sim_p.a_px_s2 = 4200;
    sim_p.damp_q16 = 65000;
    sim_p.minx = EDGEAI_BALL_R_MAX + 2;
    sim_p.miny = EDGEAI_BALL_R_MAX + 2;
    sim_p.maxx = (EDGEAI_LCD_W - 1) - (EDGEAI_BALL_R_MAX + 2);
    sim_p.maxy = (EDGEAI_LCD_H - 1) - (EDGEAI_BALL_R_MAX + 2);

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

        accel_proc_out_t aout;
        accel_proc_update(&accel_proc, (int32_t)s.x, (int32_t)s.y, &aout);

        sim_input_t sin;
        sin.ax_soft_q15 = aout.ax_soft_q15;
        sin.ay_soft_q15 = aout.ay_soft_q15;

        int iter = 0;
        while ((sim_accum_q16 >= sim_step_q16) && (iter < 6))
        {
            sim_accum_q16 -= sim_step_q16;
            iter++;
            sim_step(&world, &sin, &sim_p);
        }

        bool do_render = (render_accum_us >= render_period_us);
        if (do_render) render_accum_us = 0;

        render_hud_t hud;
        hud.accel_fail = (accel_fail > 0);
        hud.fps_last = fps_last;
        hud.npu_init_ok = npu_ok;
        hud.npu_run_enabled = (EDGEAI_ENABLE_NPU_INFERENCE ? true : false);
        hud.npu_backend = edgeai_npu_backend_char();
        if (render_world_draw(&rs, &world, do_render, &hud))
        {
            stats_frames++;
        }

        if (npu_accum_us >= 200000u)
        {
            npu_accum_us = 0;
            edgeai_npu_input_t nin;
            nin.vx_q16 = world.ball.vx_q16;
            nin.vy_q16 = world.ball.vy_q16;
            edgeai_npu_output_t nout;
            if (edgeai_npu_step(&npu, &nin, &nout))
            {
                world.ball.glint = nout.glint;
            }
        }

        if (stats_accum_us >= 1000000u)
        {
            uint32_t fps = stats_frames;
            fps_last = fps;
            stats_accum_us = 0;
            stats_frames = 0;
            int32_t cx = world.ball.x_q16 >> 16;
            int32_t cy = world.ball.y_q16 >> 16;
            PRINTF("EDGEAI: fps=%u raw=(%d,%d,%d) lp=(%d,%d) pos=(%d,%d) v=(%d,%d) glint=%u npu=%u\r\n",
                   (unsigned)fps,
                   (int)s.x, (int)s.y, (int)s.z,
                   (int)aout.ax_lp, (int)aout.ay_lp,
                   (int)cx, (int)cy,
                   (int)(world.ball.vx_q16 >> 16), (int)(world.ball.vy_q16 >> 16),
                   (unsigned)world.ball.glint,
                   (unsigned)(npu_ok ? 1u : 0u));
        }
    }
}
