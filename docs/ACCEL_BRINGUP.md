# Accel Bring-up (FXLS8974CF over mikroBUS I2C)

## Current Build Status (2026-02-10)
The accelerometer signal is used in three paths:
- Tilt control: low-pass filtered X/Y (mapped to screen axes) drives ball acceleration.
- Z scale latch: high-pass of accel magnitude `|a|` drives a persistent scale command (ball grows/shrinks and holds last scale).
- Bang impulse: a short high-pass event score can inject a one-shot velocity impulse.

## Wiring Summary
See `docs/HARDWARE.md`.

## I2C Address Probe
Accel 4 Click schematic indicates 2 possible 7-bit addresses:
- `0x18`
- `0x19`

Probe by reading:
- `WHO_AM_I` register `0x13`
- Expected value: `0x86` (FXLS8974CF)

## Required Clickboard Mode (Important)
Accel 4 Click supports both I2C and SPI. Make sure the Click board is configured for **I2C** (COMM SEL).
On some Click boards this is done via solder jumpers (not a plug-in jumper).

## Minimal Init Sequence (Draft)
1. Put device in Standby (ACTIVE=0 in `SENS_CONFIG1` @ `0x15`) before changing most config.
2. Configure range (FSR in `SENS_CONFIG1[2:1]`).
3. Configure ODR/power modes (primarily `SENS_CONFIG3`/`SENS_CONFIG2`) as needed.
4. Set ACTIVE=1.
5. Burst read `OUT_X/Y/Z` registers (`0x04..0x09`).

## Notes
- Start with LE/BE = little-endian, right-justified unpacking (common default).
- Later: add INT1 data-ready route if needed (INT_EN/INT_PIN_SEL).
