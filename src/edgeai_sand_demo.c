#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "accel4_click.h"
#include "fxls8974cf.h"
#include "sim.h"

#include "board.h"
#include "clock_config.h"
#include "fsl_clock.h"
#include "fsl_debug_console.h"
#include "fsl_lpi2c.h"
#include "pin_mux.h"

#ifndef EDGEAI_I2C
#define EDGEAI_I2C LPI2C3
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

    static uint8_t cells[160u * 120u];
    sim_grid_t grid = {.w = 160, .h = 120, .cells = cells};
    sim_rng_t rng;
    sim_rng_seed(&rng, 0xC0DEF00Du);
    seed_world(&grid);

    fxls8974_sample_t s = {0};
    uint32_t tick = 0;
    for (;;)
    {
        if (fxls8974_read_sample_12b(&dev, &s))
        {
            grav_dir_t g = grav_from_sample(&s);
            sim_step(&grid, g, &rng);
            sim_step(&grid, g, &rng);

            if ((tick++ % 60u) == 0u)
            {
                PRINTF("accel raw x=%d y=%d z=%d grav=%u\r\n", s.x, s.y, s.z, (unsigned)g);
            }
        }
    }
}
