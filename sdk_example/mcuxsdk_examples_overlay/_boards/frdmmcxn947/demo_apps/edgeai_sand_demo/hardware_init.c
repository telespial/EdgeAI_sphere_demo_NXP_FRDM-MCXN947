/*
 * Hardware init for EdgeAI Sand Demo (FRDM-MCXN947, cm33_core0).
 */

#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "fsl_clock.h"

void BOARD_InitHardware(void)
{
    // Debug console: FLEXCOMM4
    CLOCK_SetClkDiv(kCLOCK_DivFlexcom4Clk, 1u);
    CLOCK_AttachClk(BOARD_DEBUG_UART_CLK_ATTACH);

    // mikroBUS I2C: FLEXCOMM3
    CLOCK_SetClkDiv(kCLOCK_DivFlexcom3Clk, 1u);
    CLOCK_AttachClk(kFRO12M_to_FLEXCOMM3);

    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitDebugConsole();
}

