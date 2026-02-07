# Tasks

## Bring-up (Accel)
1. Confirm I2C address (`0x18` or `0x19`) by reading `WHO_AM_I` (expect `0x86`).
2. Set range (FSR) and ODR.
3. Read XYZ at a fixed rate and validate orientation.
4. Optionally enable INT1 data-ready.

## Falling Sand MVP
1. Implement grid + materials + update loop.
2. Gravity derived from accelerometer.
3. Render palette + scale to LCD.
4. Add UI to spawn materials.

## NPU Hook
1. Add optional post-process stage on low-res frame.
2. Integrate on core1 if beneficial.

