#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "accel4_click.h"
#include "fxls8974cf.h"
#include "par_lcd_s035.h"
#include "sim.h"

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

/* Accel axis mapping.
 * If tilt directions feel wrong, adjust these and reflash.
 *
 * Current default fixes common "left/right inverted" mounting.
 */
#ifndef EDGEAI_ACCEL_SWAP_XY
#define EDGEAI_ACCEL_SWAP_XY 0
#endif
#ifndef EDGEAI_ACCEL_INVERT_X
#define EDGEAI_ACCEL_INVERT_X 1
#endif
#ifndef EDGEAI_ACCEL_INVERT_Y
#define EDGEAI_ACCEL_INVERT_Y 0
#endif

static uint32_t edgeai_i2c_get_freq(void)
{
    // FC3 I2C is wired to mikroBUS on FRDM-MCXN947 (see docs/HARDWARE.md).
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

static grav_dir_t grav_from_sample(const fxls8974_sample_t *s)
{
    // Very simple 8-way mapping from (x,y). Adjust signs based on your physical mounting.
    int32_t ax = s->x;
    int32_t ay = s->y;

#if EDGEAI_ACCEL_SWAP_XY
    int32_t tmp = ax; ax = ay; ay = tmp;
#endif
#if EDGEAI_ACCEL_INVERT_X
    ax = -ax;
#endif
#if EDGEAI_ACCEL_INVERT_Y
    ay = -ay;
#endif

    const int32_t dead = 256; // rough deadzone in raw 12-bit units
    if ((ax > -dead) && (ax < dead) && (ay > -dead) && (ay < dead))
    {
        return GRAV_S;
    }

    bool right = ax > dead;
    bool left = ax < -dead;
    bool down = ay > dead;
    bool up = ay < -dead;

    if (up && right) return GRAV_NE;
    if (up && left) return GRAV_NW;
    if (down && right) return GRAV_SE;
    if (down && left) return GRAV_SW;
    if (up) return GRAV_N;
    if (down) return GRAV_S;
    if (left) return GRAV_W;
    return GRAV_E;
}

static void seed_world(sim_grid_t *g)
{
    sim_clear(g);
    // Floor
    for (uint16_t x = 0; x < g->w; x++)
    {
        sim_set(g, x, g->h - 1, MAT_METAL);
    }
    // Sand pile
    for (uint16_t y = 10; y < 40; y++)
    {
        for (uint16_t x = 50; x < 80; x++)
        {
            sim_set(g, x, y, MAT_SAND);
        }
    }
    // Water blob
    for (uint16_t y = 10; y < 30; y++)
    {
        for (uint16_t x = 90; x < 120; x++)
        {
            sim_set(g, x, y, MAT_WATER);
        }
    }

    // Metal ball seed (single-cell for now).
    sim_set(g, 30, 20, MAT_BALL);
}

int main(void)
{
    BOARD_InitHardware();

    // Init I2C3 (FC3 pins).
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
        PRINTF("FXLS8974CF not found on I2C (tried 0x18/0x19). WHO_AM_I=0x%02x\r\n", who);
        for (;;)
        {
        }
    }

    PRINTF("FXLS8974CF found at 0x%02x (WHO_AM_I=0x%02x)\r\n", dev.addr7, who);

    // Minimal configuration: set FSR and enable ACTIVE.
    (void)fxls8974_set_active(&dev, false);
    (void)fxls8974_set_fsr(&dev, FXLS8974_FSR_4G);
    (void)fxls8974_set_active(&dev, true);

    if (!par_lcd_s035_init())
    {
        PRINTF("LCD init failed\r\n");
        for (;;)
        {
        }
    }
    par_lcd_s035_fill(0x0000u);

    static uint8_t cells[160u * 120u];
    sim_grid_t grid = {.w = 160, .h = 120, .cells = cells};
    sim_rng_t rng;
    sim_rng_seed(&rng, 0xC0DEF00Du);
    seed_world(&grid);

    fxls8974_sample_t s = {0};
    uint32_t tick = 0;

    /* Motion trail state at sim resolution. */
    static uint8_t prev_cells[160u * 120u];
    static uint8_t trails[160u * 120u];
    memcpy(prev_cells, grid.cells, sizeof(prev_cells));

    /* Ball state (single cell, fixed-point 24.8). */
    int32_t ball_x_fp = 30 << 8;
    int32_t ball_y_fp = 20 << 8;
    int32_t ball_vx_fp = 0;
    int32_t ball_vy_fp = 0;
    uint16_t ball_x = 30;
    uint16_t ball_y = 20;

    for (;;)
    {
        if (fxls8974_read_sample_12b(&dev, &s))
        {
            grav_dir_t g = grav_from_sample(&s);
            sim_step(&grid, g, &rng);
            sim_step(&grid, g, &rng);

            /* Ball "physics": inertia + swaps with sand/water; bounces off metal/bounds. */
            {
                int gx = 0, gy = 1;
                switch (g)
                {
                    case GRAV_N:  gx = 0;  gy = -1; break;
                    case GRAV_NE: gx = 1;  gy = -1; break;
                    case GRAV_E:  gx = 1;  gy = 0;  break;
                    case GRAV_SE: gx = 1;  gy = 1;  break;
                    case GRAV_S:  gx = 0;  gy = 1;  break;
                    case GRAV_SW: gx = -1; gy = 1;  break;
                    case GRAV_W:  gx = -1; gy = 0;  break;
                    case GRAV_NW: gx = -1; gy = -1; break;
                    default: break;
                }

                /* Acceleration strength in fp units. */
                const int32_t accel = 18;
                ball_vx_fp += gx * accel;
                ball_vy_fp += gy * accel;

                /* Damping. */
                ball_vx_fp = (ball_vx_fp * 245) >> 8;
                ball_vy_fp = (ball_vy_fp * 245) >> 8;

                int32_t nx_fp = ball_x_fp + ball_vx_fp;
                int32_t ny_fp = ball_y_fp + ball_vy_fp;

                /* Bounds + bounce. */
                int32_t minx = 0, miny = 0;
                int32_t maxx = ((int32_t)grid.w - 1) << 8;
                int32_t maxy = ((int32_t)grid.h - 1) << 8;

                if (nx_fp < minx) { nx_fp = minx; ball_vx_fp = -(ball_vx_fp >> 1); }
                if (nx_fp > maxx) { nx_fp = maxx; ball_vx_fp = -(ball_vx_fp >> 1); }
                if (ny_fp < miny) { ny_fp = miny; ball_vy_fp = -(ball_vy_fp >> 1); }
                if (ny_fp > maxy) { ny_fp = maxy; ball_vy_fp = -(ball_vy_fp >> 1); }

                uint16_t nx = (uint16_t)(nx_fp >> 8);
                uint16_t ny = (uint16_t)(ny_fp >> 8);

                if (nx != ball_x || ny != ball_y)
                {
                    material_t dst = sim_get(&grid, nx, ny);
                    if (dst == MAT_METAL)
                    {
                        /* Bounce back. */
                        ball_vx_fp = -(ball_vx_fp >> 1);
                        ball_vy_fp = -(ball_vy_fp >> 1);
                    }
                    else
                    {
                        /* Swap into destination; leave displaced material behind. */
                        sim_set(&grid, ball_x, ball_y, dst);
                        sim_set(&grid, nx, ny, MAT_BALL);
                        ball_x = nx;
                        ball_y = ny;
                        ball_x_fp = nx_fp;
                        ball_y_fp = ny_fp;
                    }
                }
            }

            /* Render at a lower cadence than sim updates. */
            if ((tick % 4u) == 0u)
            {
                /* Update trails (decay + stamp changed cells). */
                for (uint32_t i = 0; i < (uint32_t)grid.w * (uint32_t)grid.h; i++)
                {
                    uint8_t t = trails[i];
                    trails[i] = (t > 8u) ? (uint8_t)(t - 8u) : 0u;
                    if (grid.cells[i] != prev_cells[i])
                    {
                        trails[i] = 200u;
                        prev_cells[i] = grid.cells[i];
                    }
                }

                par_lcd_s035_render_grid(&grid, trails, tick);
            }

            if ((tick++ % 60u) == 0u)
            {
                /* fsl_debug_console printf formatting can be picky; cast explicitly. */
                long ax = (long)((int32_t)s.x);
                long ay = (long)((int32_t)s.y);
                long az = (long)((int32_t)s.z);
                PRINTF("accel raw x=%ld y=%ld z=%ld grav=%u\r\n", ax, ay, az, (unsigned)g);
            }
        }
    }
}
