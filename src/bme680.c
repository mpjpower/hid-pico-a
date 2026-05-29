#include "bme680.h"

#include "hardware/i2c.h"
#include "i2c_interface.h"

#include <stdint.h>

// BME680 modules commonly use 0x76 or 0x77 depending on the SDO pin.
// The named device uses the more common 0x76 address; 0x77 can still be
// accessed directly through the numeric device fallback in SetRegs/GetRegs.
#define BME680_ADDR  0x76

static int bme680_write_reg(uint8_t addr, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    int ret = i2c_write_blocking(i2c0, addr, buf, 2, false);
    return (ret == 2) ? 0 : -1;
}

static int bme680_read_reg(uint8_t addr, uint8_t reg, uint8_t *val) {
    int ret = i2c_write_blocking(i2c0, addr, &reg, 1, true);
    if (ret != 1) {
        return -1;
    }

    ret = i2c_read_blocking(i2c0, addr, val, 1, false);
    return (ret == 1) ? 0 : -1;
}

static const i2c_device_t bme680_device = {
    .name = "bme680",
    .addr = BME680_ADDR,
    .write_reg = bme680_write_reg,
    .read_reg = bme680_read_reg,
};

void bme680_register(void) {
    i2c_register_device(&bme680_device);
}