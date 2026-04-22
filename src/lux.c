#include "lux.h"
#include "i2c_interface.h"
#include "hardware/i2c.h"
#include <stdint.h>

// TSL2591 I2C address (fixed, cannot be changed)
#define TSL2591_ADDR  0x29

// Command byte: bit7=1 (command), bits5-4=00 (normal), bits3-0=register
#define TSL2591_CMD   0xA0

static int tsl2591_write_reg(uint8_t addr, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { TSL2591_CMD | reg, val };
    int ret = i2c_write_blocking(i2c0, addr, buf, 2, false);
    return (ret == 2) ? 0 : -1;
}

static int tsl2591_read_reg(uint8_t addr, uint8_t reg, uint8_t *val) {
    uint8_t cmd = TSL2591_CMD | reg;
    int ret = i2c_write_blocking(i2c0, addr, &cmd, 1, true);
    if (ret != 1) return -1;
    ret = i2c_read_blocking(i2c0, addr, val, 1, false);
    return (ret == 1) ? 0 : -1;
}

static const i2c_device_t tsl2591_device = {
    .name      = "tsl2591",
    .addr      = TSL2591_ADDR,
    .write_reg = tsl2591_write_reg,
    .read_reg  = tsl2591_read_reg,
};

void lux_register(void) {
    i2c_register_device(&tsl2591_device);
}
