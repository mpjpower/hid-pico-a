#ifndef I2C_INTERFACE_H
#define I2C_INTERFACE_H

#include <stdbool.h>
#include <stdint.h>

#define I2C_MAX_DEVICES 8

// Device descriptor: each supported device registers one of these
typedef struct {
    const char *name;
    uint8_t     addr;
    int (*write_reg)(uint8_t addr, uint8_t reg, uint8_t val);
    int (*read_reg) (uint8_t addr, uint8_t reg, uint8_t *val);
} i2c_device_t;

// Initialise the I2C bus (i2c0, GP4/GP5, 400 kHz)
void i2c_bus_init(void);

// Register a device so SetRegs/GetRegs can find it by name
void i2c_register_device(const i2c_device_t *dev);

// Write 'count' register/value pairs to the named device.
// regs[i] and vals[i] are the register address and value for each pair.
// Returns 0 on success, -1 on failure.
int SetRegs(const char *device, uint8_t *regs, uint8_t *vals, int count);

// Read 'count' registers from the named device into out[].
// regs[i] is the register address; result is stored in out[i].
// Returns 0 on success, -1 on failure.
int GetRegs(const char *device, uint8_t *regs, int count, uint8_t *out);

// Returns true if a device with this name is registered.
bool i2c_device_known(const char *device);

// Returns true if the device ACKs on the I2C bus.
bool i2c_device_probe(const char *device);

#endif // I2C_INTERFACE_H
