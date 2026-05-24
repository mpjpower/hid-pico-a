#include "adxl343.h"

#include "hardware/i2c.h"
#include "i2c_interface.h"

#include <stdint.h>

#define ADXL343_ADDR  0x53

static int adxl343_write_reg(uint8_t addr, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    int ret = i2c_write_blocking(i2c0, addr, buf, 2, false);
    return (ret == 2) ? 0 : -1;
}

static int adxl343_read_reg(uint8_t addr, uint8_t reg, uint8_t *val) {
    int ret = i2c_write_blocking(i2c0, addr, &reg, 1, true);
    if (ret != 1) {
        return -1;
    }

    ret = i2c_read_blocking(i2c0, addr, val, 1, false);
    return (ret == 1) ? 0 : -1;
}

static const i2c_device_t adxl343_device = {
    .name = "adxl343",
    .addr = ADXL343_ADDR,
    .write_reg = adxl343_write_reg,
    .read_reg = adxl343_read_reg,
};

void adxl343_register(void) {
    i2c_register_device(&adxl343_device);
}