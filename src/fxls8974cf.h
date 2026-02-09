#pragma once

#include <stdbool.h>
#include <stdint.h>

// Minimal FXLS8974CF register definitions (extend as needed).
// Source: datasheets/sensors/nxp/fxls8974cf/FXLS8974CF.pdf

#define FXLS8974_REG_INT_STATUS   0x00u
#define FXLS8974_REG_TEMP_OUT     0x01u
#define FXLS8974_REG_OUT_X_LSB    0x04u
#define FXLS8974_REG_OUT_X_MSB    0x05u
#define FXLS8974_REG_OUT_Y_LSB    0x06u
#define FXLS8974_REG_OUT_Y_MSB    0x07u
#define FXLS8974_REG_OUT_Z_LSB    0x08u
#define FXLS8974_REG_OUT_Z_MSB    0x09u
#define FXLS8974_REG_PROD_REV     0x12u
#define FXLS8974_REG_WHO_AM_I     0x13u
#define FXLS8974_REG_SYS_MODE     0x14u
#define FXLS8974_REG_SENS_CONFIG1 0x15u
#define FXLS8974_REG_SENS_CONFIG2 0x16u
#define FXLS8974_REG_SENS_CONFIG3 0x17u
#define FXLS8974_REG_SENS_CONFIG4 0x18u
#define FXLS8974_REG_SENS_CONFIG5 0x19u

#define FXLS8974_WHO_AM_I_VALUE   0x86u

typedef enum {
  FXLS8974_FSR_2G  = 0,
  FXLS8974_FSR_4G  = 1,
  FXLS8974_FSR_8G  = 2,
  FXLS8974_FSR_16G = 3,
} fxls8974_fsr_t;

typedef struct {
  int16_t x;  // signed 12-bit sample left in int16 container
  int16_t y;
  int16_t z;
} fxls8974_sample_t;

// I2C transfer hooks (platform-specific).
// reg is 8-bit. For read/write, use the common "reg address then data" pattern.
typedef bool (*fxls8974_i2c_write_fn)(uint8_t addr7, uint8_t reg, const uint8_t *data, uint32_t len);
typedef bool (*fxls8974_i2c_read_fn)(uint8_t addr7, uint8_t reg, uint8_t *data, uint32_t len);

typedef struct {
  uint8_t addr7; // 7-bit I2C address (0x18 or 0x19 on Accel 4 Click)
  fxls8974_i2c_write_fn write;
  fxls8974_i2c_read_fn read;
} fxls8974_dev_t;

bool fxls8974_read_whoami(const fxls8974_dev_t *dev, uint8_t *whoami);
bool fxls8974_set_active(const fxls8974_dev_t *dev, bool active);
bool fxls8974_set_fsr(const fxls8974_dev_t *dev, fxls8974_fsr_t fsr);
bool fxls8974_read_sample_12b(const fxls8974_dev_t *dev, fxls8974_sample_t *out);
