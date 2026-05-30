#ifndef BME68X_PICO_H
#define BME68X_PICO_H

#include <stdint.h>
#include <stdbool.h>

// Sensor reading structure: temperature (°C), pressure (hPa), humidity (%), gas_resistance (kΩ)
typedef struct {
    float temperature;
    float pressure;
    float humidity;
    float gas_resistance;
    bool gas_valid;        // true if gas measurement is valid
    uint8_t measurement_index;  // index of the measurement
} bme68x_reading_t;

// Configuration structure for sensor parameters
typedef struct {
    uint8_t gas_enabled;       // 1 to enable gas measurement, 0 to disable
    uint16_t heater_dur;       // Heater duration in ms (e.g., 150)
    uint16_t heater_temp;      // Heater temperature in °C (e.g., 320)
    uint8_t filter_coeff;      // Filter coefficient (0-7, e.g., 3)
    uint8_t osr_hum;           // Humidity oversampling (0-4, e.g., 2 for 4x)
    uint8_t osr_temp;          // Temperature oversampling (0-4, e.g., 3 for 8x)
    uint8_t osr_pres;          // Pressure oversampling (0-4, e.g., 4 for 16x)
    uint8_t mode;              // Mode: 0=sleep, 1=forced, 2=parallel, 3=sequential
} bme68x_config_t;

// Initialize the sensor (detect variant, read calibration)
// Returns 0 on success, -1 on failure
int bme68x_pico_init(void);

// Configure the sensor with the given parameters
// Returns 0 on success, -1 on failure
int bme68x_pico_configure(const bme68x_config_t *cfg);

// Trigger a measurement in forced mode and read the result
// Returns 0 on success, -1 on failure
// The reading is placed into 'out'
int bme68x_pico_read(bme68x_reading_t *out);

#endif // BME68X_PICO_H
