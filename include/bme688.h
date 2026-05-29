#ifndef BME688_H
#define BME688_H

// Register the BME688 sensor with the I2C device registry.
// Call this once during initialisation before using SetRegs/GetRegs
// with device name "bme688".
void bme688_register(void);

#endif // BME688_H