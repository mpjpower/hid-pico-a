#include "i2c_interface.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include <stdlib.h>
#include <string.h>

#define I2C_BUS      i2c0
#define I2C_SDA_PIN  4
#define I2C_SCL_PIN  5
#define I2C_BAUD     100000
#define I2C_SCAN_MIN_ADDR 0x08
#define I2C_SCAN_MAX_ADDR 0x77
#define I2C_SCAN_PROBE_TRIES 3

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

static bool parse_i2c_addr(const char *name, uint8_t *addr_out) {
    char *endptr;
    unsigned long value;

    if (!name || !*name || !addr_out) {
        return false;
    }

    value = strtoul(name, &endptr, 0);
    if (*endptr != '\0' || value > 0x7Fu) {
        return false;
    }

    *addr_out = (uint8_t) value;
    return true;
}

static bool resolve_i2c_addr(const char *device, uint8_t *addr_out) {
    const i2c_device_t *dev = find_device(device);

    if (dev) {
        *addr_out = dev->addr;
        return true;
    }

    return parse_i2c_addr(device, addr_out);
}

static bool i2c_addr_ack(uint8_t addr) {
    uint8_t dummy = 0;
    int ret = i2c_write_blocking(I2C_BUS, addr, &dummy, 1, false);
    return ret == 1;
}

static bool i2c_addr_ack_stable(uint8_t addr) {
    int ack_count = 0;

    for (int i = 0; i < I2C_SCAN_PROBE_TRIES; i++) {
        if (i2c_addr_ack(addr)) {
            ack_count++;
        }
    }

    return ack_count >= ((I2C_SCAN_PROBE_TRIES / 2) + 1);
}

int SetRegs(const char *device, uint8_t *regs, uint8_t *vals, int count) {
    const i2c_device_t *dev = find_device(device);
    uint8_t raw_addr = 0;

    if (!dev && !parse_i2c_addr(device, &raw_addr)) {
        return -1;
    }

    for (int i = 0; i < count; i++) {
        if (dev) {
            if (dev->write_reg(dev->addr, regs[i], vals[i]) != 0) {
                return -1;
            }
        } else {
            uint8_t buf[2] = { regs[i], vals[i] };
            int ret = i2c_write_blocking(I2C_BUS, raw_addr, buf, 2, false);
            if (ret != 2) {
                return -1;
            }
        }
    }

    return 0;
}

int WriteBytes(const char *device, const uint8_t *bytes, int count) {
    uint8_t raw_addr = 0;

    if (count <= 0 || !bytes || !resolve_i2c_addr(device, &raw_addr)) {
        return -1;
    }

    int ret = i2c_write_blocking(I2C_BUS, raw_addr, bytes, (size_t) count, false);
    return (ret == count) ? 0 : -1;
}

int GetRegs(const char *device, uint8_t *regs, int count, uint8_t *out) {
    const i2c_device_t *dev = find_device(device);
    uint8_t raw_addr = 0;

    if (!dev && !parse_i2c_addr(device, &raw_addr)) {
        return -1;
    }

    for (int i = 0; i < count; i++) {
        if (dev) {
            if (dev->read_reg(dev->addr, regs[i], &out[i]) != 0) {
                return -1;
            }
        } else {
            int ret = i2c_write_blocking(I2C_BUS, raw_addr, &regs[i], 1, true);
            if (ret != 1) {
                return -1;
            }

            ret = i2c_read_blocking(I2C_BUS, raw_addr, &out[i], 1, false);
            if (ret != 1) {
                return -1;
            }
        }
    }

    return 0;
}

bool i2c_device_known(const char *device) {
    uint8_t addr = 0;
    return resolve_i2c_addr(device, &addr);
}

bool i2c_device_probe(const char *device) {
    uint8_t addr;

    if (!resolve_i2c_addr(device, &addr)) {
        return false;
    }

    return i2c_addr_ack(addr);
}

int i2c_scan_bitmap_hex(char *out, size_t out_size) {
    static const char HEX[] = "0123456789ABCDEF";

    if (!out || out_size < 33) {
        return -1;
    }

    for (int nibble = 0; nibble < 32; nibble++) {
        uint8_t bits = 0;

        for (int bit = 0; bit < 4; bit++) {
            uint8_t addr = (uint8_t) (nibble * 4 + bit);
            if (addr >= I2C_SCAN_MIN_ADDR && addr <= I2C_SCAN_MAX_ADDR && i2c_addr_ack_stable(addr)) {
                bits |= (uint8_t) (1u << bit);
            }
        }

        out[nibble] = HEX[bits & 0x0F];
    }

    out[32] = '\0';
    return 0;
}
