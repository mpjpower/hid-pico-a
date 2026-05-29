#ifndef BME680_H
#define BME680_H

// Register the BME680 sensor with the I2C device registry.
// Call this once during initialisation before using SetRegs/GetRegs
// with device name "bme680".
void bme680_register(void);

#endif // BME680_H