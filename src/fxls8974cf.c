#include "fxls8974cf.h"

static bool rd8(const fxls8974_dev_t *dev, uint8_t reg, uint8_t *v) {
  return dev && dev->read && dev->read(dev->addr7, reg, v, 1);
}

static bool wr8(const fxls8974_dev_t *dev, uint8_t reg, uint8_t v) {
  return dev && dev->write && dev->write(dev->addr7, reg, &v, 1);
}

bool fxls8974_read_whoami(const fxls8974_dev_t *dev, uint8_t *whoami) {
  if (!whoami) return false;
  return rd8(dev, FXLS8974_REG_WHO_AM_I, whoami);
}

bool fxls8974_set_active(const fxls8974_dev_t *dev, bool active) {
  uint8_t v = 0;
  if (!rd8(dev, FXLS8974_REG_SENS_CONFIG1, &v)) return false;
  if (active) {
    v |= 0x01u;
  } else {
    v &= (uint8_t)~0x01u;
  }
  return wr8(dev, FXLS8974_REG_SENS_CONFIG1, v);
}

bool fxls8974_set_fsr(const fxls8974_dev_t *dev, fxls8974_fsr_t fsr) {
  // SENS_CONFIG1[2:1] = FSR
  uint8_t v = 0;
  if (!rd8(dev, FXLS8974_REG_SENS_CONFIG1, &v)) return false;
  v &= (uint8_t)~(0x03u << 1);
  v |= (uint8_t)(((uint8_t)fsr & 0x03u) << 1);
  return wr8(dev, FXLS8974_REG_SENS_CONFIG1, v);
}

static int16_t unpack_12b_little_endian_right_just(uint8_t lsb, uint8_t msb) {
  // In LE_BE=0 mode, sample bits are [11:0] with:
  // LSB reg: bits [7:0] = data[7:0]
  // MSB reg: bits [3:0] = data[11:8]
  uint16_t u = (uint16_t)lsb | ((uint16_t)(msb & 0x0Fu) << 8);
  // sign extend 12-bit
  if (u & 0x0800u) u |= 0xF000u;
  return (int16_t)u;
}

bool fxls8974_read_sample_12b(const fxls8974_dev_t *dev, fxls8974_sample_t *out) {
  if (!out) return false;

  uint8_t raw[6] = {0};
  if (!dev || !dev->read) return false;

  // Read OUT_X..OUT_Z in one burst (auto-increment is supported).
  if (!dev->read(dev->addr7, FXLS8974_REG_OUT_X_LSB, raw, sizeof(raw))) return false;

  out->x = unpack_12b_little_endian_right_just(raw[0], raw[1]);
  out->y = unpack_12b_little_endian_right_just(raw[2], raw[3]);
  out->z = unpack_12b_little_endian_right_just(raw[4], raw[5]);
  return true;
}

