#ifndef LUX_H
#define LUX_H

// Register the TSL2591 light sensor with the I2C device registry.
// Call this once during initialisation before using SetRegs/GetRegs
// with device name "tsl2591".
void lux_register(void);

#endif // LUX_H
