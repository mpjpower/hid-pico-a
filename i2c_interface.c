#include "i2c_interface.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include <string.h>

#define I2C_BUS      i2c0
#define I2C_SDA_PIN  4
#define I2C_SCL_PIN  5
#define I2C_BAUD     100000

static const i2c_device_t *devices[I2C_MAX_DEVICES];
static int device_count = 0;

void i2c_bus_init(void) {
    i2c_init(I2C_BUS, I2C_BAUD);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
}

void i2c_register_device(const i2c_device_t *dev) {
    if (device_count < I2C_MAX_DEVICES) {
        devices[device_count++] = dev;
    }
}

static const i2c_device_t *find_device(const char *name) {
    for (int i = 0; i < device_count; i++) {
        if (strcmp(devices[i]->name, name) == 0) {
            return devices[i];
        }
    }
    return NULL;
}

int SetRegs(const char *device, uint8_t *regs, uint8_t *vals, int count) {
    const i2c_device_t *dev = find_device(device);
    if (!dev) return -1;
    for (int i = 0; i < count; i++) {
        if (dev->write_reg(dev->addr, regs[i], vals[i]) != 0) return -1;
    }
    return 0;
}

int GetRegs(const char *device, uint8_t *regs, int count, uint8_t *out) {
    const i2c_device_t *dev = find_device(device);
    if (!dev) return -1;
    for (int i = 0; i < count; i++) {
        if (dev->read_reg(dev->addr, regs[i], &out[i]) != 0) return -1;
    }
    return 0;
}

bool i2c_device_known(const char *device) {
    return find_device(device) != NULL;
}

bool i2c_device_probe(const char *device) {
    const i2c_device_t *dev = find_device(device);
    if (!dev) return false;
    int ret = i2c_write_blocking(I2C_BUS, dev->addr, NULL, 0, false);
    return ret >= 0;
}
