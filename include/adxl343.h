#ifndef ADXL343_H
#define ADXL343_H

// Register the ADXL343 accelerometer with the I2C device registry.
// Call this once during initialisation before using SetRegs/GetRegs
// with device name "adxl343".
void adxl343_register(void);

#endif // ADXL343_H