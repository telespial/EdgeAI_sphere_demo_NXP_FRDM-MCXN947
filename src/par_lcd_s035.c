#include "par_lcd_s035.h"

#include <string.h>

#include "fsl_common.h"
#include "fsl_clock.h"
#include "fsl_debug_console.h"
#include "fsl_gpio.h"

#include "fsl_flexio_mculcd.h"
#include "fsl_dbi_flexio_edma.h"
#include "fsl_st7796s.h"

#include "board.h"
#include "pin_mux.h"

#include "postfx.h"

/* LCD shield used by the FRDM-MCXN947 LVGL examples: PAR-LCD-S035.
 * Controller: ST7796S, 480x320, 8080 parallel via FlexIO0.
 *
 * Pin mapping follows MCUX SDK board support:
 *   examples/_boards/frdmmcxn947/lvgl_examples/lvgl_support/lvgl_support_board.h
 */
#define EDGEAI_LCD_WIDTH  480
#define EDGEAI_LCD_HEIGHT 320

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

bool par_lcd_s035_init(void)
{
    /* Configure LCD pins (FlexIO bus + CS/RS/RST). */
    BOARD_InitLcdPins();

    /* FlexIO MCU LCD device init. */
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

    /* Hardware reset before controller init. */
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

static void lcd_wait_write_done(void)
{
    while (!s_memWriteDone)
    {
    }
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

static inline uint16_t mat_to_rgb565(uint8_t m)
{
    switch (m)
    {
        case MAT_SAND:  return 0xF6A0u; /* warm yellow */
        case MAT_WATER: return 0x03FFu; /* cyan-ish */
        case MAT_METAL: return 0x8410u; /* grey */
        case MAT_BALL:  return 0xFFFFu; /* white */
        default:        return 0x0000u; /* black */
    }
}

static inline uint16_t rgb565_add(uint16_t c, int dr, int dg, int db)
{
    int r = (c >> 11) & 0x1F;
    int g = (c >> 5)  & 0x3F;
    int b = (c >> 0)  & 0x1F;
    r += dr; g += dg; b += db;
    if (r < 0) { r = 0; }
    if (r > 31) { r = 31; }
    if (g < 0) { g = 0; }
    if (g > 63) { g = 63; }
    if (b < 0) { b = 0; }
    if (b > 31) { b = 31; }
    return (uint16_t)((r << 11) | (g << 5) | (b << 0));
}

static inline uint32_t hash32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

void par_lcd_s035_render_grid(const sim_grid_t *grid, const uint8_t *trail, uint32_t frame)
{
    if (!grid || !grid->cells || grid->w == 0 || grid->h == 0)
    {
        return;
    }

    static uint16_t line[EDGEAI_LCD_WIDTH];

    for (uint32_t y = 0; y < EDGEAI_LCD_HEIGHT; y++)
    {
        uint32_t sy = (y * (uint32_t)grid->h) / EDGEAI_LCD_HEIGHT;
        if (sy >= grid->h) sy = grid->h - 1u;

        const uint8_t *row = &grid->cells[sy * (uint32_t)grid->w];
        for (uint32_t x = 0; x < EDGEAI_LCD_WIDTH; x++)
        {
            uint32_t sx = (x * (uint32_t)grid->w) / EDGEAI_LCD_WIDTH;
            if (sx >= grid->w) sx = grid->w - 1u;
            uint8_t m = row[sx];
            uint16_t c = mat_to_rgb565(m);

            /* Stylized shading: light from upper-left. */
            if (m != MAT_EMPTY)
            {
                bool empty_up   = (sy == 0) ? true : (grid->cells[(sy - 1u) * (uint32_t)grid->w + sx] == MAT_EMPTY);
                bool empty_left = (sx == 0) ? true : (row[sx - 1u] == MAT_EMPTY);
                bool empty_dn   = (sy + 1u >= grid->h) ? true : (grid->cells[(sy + 1u) * (uint32_t)grid->w + sx] == MAT_EMPTY);
                bool empty_rt   = (sx + 1u >= grid->w) ? true : (row[sx + 1u] == MAT_EMPTY);

                if (empty_up || empty_left)
                {
                    c = rgb565_add(c, 2, 4, 2);
                }
                if (empty_dn || empty_rt)
                {
                    c = rgb565_add(c, -2, -4, -2);
                }
            }

            /* Material-specific "feel". */
            uint32_t h = hash32((frame * 131u) ^ (sx * 911u) ^ (sy * 3571u));
            if (m == MAT_WATER)
            {
                /* shimmer highlights */
                int s = (int)((h >> 28) & 0xF) - 7; /* [-7..8] */
                c = rgb565_add(c, 0, s, s);
            }
            else if (m == MAT_SAND)
            {
                /* dither/noise */
                int n = (int)((h >> 30) & 0x3) - 1; /* [-1..2] */
                c = rgb565_add(c, n, n * 2, 0);
            }
            else if (m == MAT_METAL)
            {
                /* subtle specular streaks */
                if (((h >> 27) & 0x7) == 0)
                {
                    c = rgb565_add(c, 3, 6, 3);
                }
            }

            /* Motion trails (optional). */
            if (trail)
            {
                uint8_t t = trail[sy * (uint32_t)grid->w + sx];
                if (t)
                {
                    int boost = (int)(t >> 5); /* 0..7 */
                    c = rgb565_add(c, boost, boost * 2, boost);
                }
            }

            line[x] = c;
        }

        postfx_apply_line_rgb565(line, EDGEAI_LCD_WIDTH, y, frame);

        ST7796S_SelectArea(&s_lcdHandle, 0, (uint16_t)y, EDGEAI_LCD_WIDTH - 1u, (uint16_t)y);
        s_memWriteDone = false;
        ST7796S_WritePixels(&s_lcdHandle, line, EDGEAI_LCD_WIDTH);
        lcd_wait_write_done();
    }
}
