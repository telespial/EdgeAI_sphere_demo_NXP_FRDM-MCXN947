# EdgeAI Sand Demo (FRDM-MCXN947)

This is a sandbox app intended to evolve into a "falling sand" simulation (sand/water/metal/ball)
driven by an external accelerometer.

Hardware assumption:
- FRDM-MCXN947 + MikroShield + Accel 4 Click (FXLS8974CF) on mikroBUS
- I2C via FC3 (mikroBUS SDA/SCL), address 0x18 or 0x19, `WHO_AM_I` = 0x86

Code lives in the top-level `codemaster` project repo:
- `projects/nxp/frdm-mcxn947/EdgeAI_sand_demo/src/`
- `projects/nxp/frdm-mcxn947/EdgeAI_sand_demo/docs/`

This example's CMake pulls sources from that folder so development stays in the project repo.

