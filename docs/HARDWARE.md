# Hardware (FRDM-MCXN947 + MikroShield + Accel 4 Click)

## Current Build Status (2026-02-10)
Active demo:
- PAR-LCD-S035 shows a dune background with a reflective silver ball, trails, shadow, HUD, and a latched Z scale (grow/shrink).
- Accel 4 Click (FXLS8974CF) provides tilt input for X/Y control and vertical-motion input for Z scale.
- MCXN947 NPU (Neutron) can be enabled to generate a 0..255 `glint` modulation signal for specular highlights.

## Stack
- Board: FRDM-MCXN947
- Shield: MikroShield (routes mikroBUS to FRDM)
- Click: Accel 4 Click (MIKROE-4630)
- Sensor on Click schematic: NXP FXLS8974CF (matches `datasheets/sensors/.../FXLS8974CF.pdf`)

## Interface Choice
Assume I2C.

Accel 4 Click supports both I2C and SPI via **solder jumpers** on the Click board (not always a visible "plug-in" jumper).

## mikroBUS Signal Mapping (from FRDM schematic text)
The FRDM schematic `datasheets/90818-MCXN947SH.pdf` routes mikroBUS signals as follows:

- mikroBUS `SCL` -> `P1_1 / FC3_I2C_SCL`
- mikroBUS `SDA` -> `P1_0 / FC3_I2C_SDA`
- mikroBUS `INT` -> `P5_7 / ME_INT` (Accel INT1 on the Click)
- mikroBUS `PWM` -> `P3_19 / ME_PWM` (unused by Accel 4 Click)
- mikroBUS `CS/SCK/MISO/MOSI` -> `P3_23/P3_21/P3_22/P3_20` (SPI path if ever needed)

The Click board schematic wires:
- mikroBUS `INT` -> FXLS8974CF `INT1/MOT_DET`
- mikroBUS `AN`  -> FXLS8974CF `INT2/TRG/BOOT`

## I2C Address
Accel 4 Click schematic indicates two possible I2C addresses:
- `0x18`
- `0x19`

Probe both and validate by reading `WHO_AM_I` (FXLS8974CF: `0x86` at register `0x13`).

## Local Datasheets
- Click schematic: `datasheets/sensors/mikroe/accel-4-click/Accel_4_click_v100_Schematic.PDF`
- Sensor datasheet: `datasheets/sensors/nxp/fxls8974cf/FXLS8974CF.pdf`

## MRD
- FXLS8974CF (Base): `mrdatasheets/devices/sensors/nxp_fxls8974cf_base.json`
- Accel 4 Click (Base): `mrdatasheets/devices/modules/mikroe_accel_4_click_base.json`
