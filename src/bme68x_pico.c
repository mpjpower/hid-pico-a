#include "bme68x_pico.h"
#include "bme68x.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

// I2C interface
#define I2C_BUS      i2c0
#define BME68X_ADDR  0x76

// Global state
static struct bme68x_dev bme68x_dev;
static bool initialized = false;

// I2C read callback for Bosch API
static int8_t bme68x_i2c_read(uint8_t reg_addr, uint8_t *data, uint32_t len, void *intf_ptr) {
    (void) intf_ptr;  // unused
    
    // Write register address with repeated start
    int ret = i2c_write_blocking(I2C_BUS, BME68X_ADDR, &reg_addr, 1, true);
    if (ret != 1) return -1;
    
    // Read data bytes
    ret = i2c_read_blocking(I2C_BUS, BME68X_ADDR, data, len, false);
    if (ret != (int)len) return -1;
    
    return 0;
}

// I2C write callback for Bosch API
static int8_t bme68x_i2c_write(uint8_t reg_addr, const uint8_t *data, uint32_t len, void *intf_ptr) {
    (void) intf_ptr;  // unused
    
    // Allocate buffer for register + data
    uint8_t *buf = malloc(len + 1);
    if (!buf) return -1;
    
    buf[0] = reg_addr;
    memcpy(buf + 1, data, len);
    
    // Write in one transaction
    int ret = i2c_write_blocking(I2C_BUS, BME68X_ADDR, buf, len + 1, false);
    free(buf);
    
    if (ret != (int)(len + 1)) return -1;
    return 0;
}

// Delay callback for Bosch API (in microseconds)
static void bme68x_delay_us(uint32_t us, void *intf_ptr) {
    (void) intf_ptr;  // unused
    sleep_us(us);
}

// Initialize the sensor
int bme68x_pico_init(void) {
    int8_t ret;
    
    if (initialized) return 0;  // Already initialized
    
    // Set up device structure
    bme68x_dev.intf = BME68X_I2C_INTF;
    bme68x_dev.read = bme68x_i2c_read;
    bme68x_dev.write = bme68x_i2c_write;
    bme68x_dev.delay_us = bme68x_delay_us;
    bme68x_dev.intf_ptr = NULL;
    
    // Initialize the sensor
    ret = bme68x_init(&bme68x_dev);
    if (ret != BME68X_OK) return -1;
    
    // Set default ambient temperature for gas calculations
    bme68x_dev.amb_temp = 25;
    
    initialized = true;
    return 0;
}

// Configure the sensor
int bme68x_pico_configure(const bme68x_config_t *cfg) {
    int8_t ret;
    struct bme68x_conf conf;
    struct bme68x_heatr_conf heatr_conf;
    
    if (!initialized) return -1;
    
    // Get current configuration
    ret = bme68x_get_conf(&conf, &bme68x_dev);
    if (ret != BME68X_OK) return -1;
    
    // Apply user configuration
    conf.filter = cfg->filter_coeff;
    conf.odr = 0;  // ODR not used in forced mode
    conf.os_hum = cfg->osr_hum;
    conf.os_temp = cfg->osr_temp;
    conf.os_pres = cfg->osr_pres;
    
    // Set heater configuration
    memset(&heatr_conf, 0, sizeof(heatr_conf));
    heatr_conf.enable = cfg->gas_enabled ? BME68X_ENABLE : BME68X_DISABLE;
    heatr_conf.heatr_temp = cfg->heater_temp;
    heatr_conf.heatr_dur = cfg->heater_dur;
    
    // Apply configuration
    ret = bme68x_set_conf(&conf, &bme68x_dev);
    if (ret != BME68X_OK) return -1;
    
    ret = bme68x_set_heatr_conf(BME68X_FORCED_MODE, &heatr_conf, &bme68x_dev);
    if (ret != BME68X_OK) return -1;
    
    return 0;
}

// Read sensor data in forced mode
int bme68x_pico_read(bme68x_reading_t *out) {
    int8_t ret;
    struct bme68x_data data;
    uint8_t n_data = 1;
    
    if (!initialized || !out) return -1;
    
    // Set forced mode
    ret = bme68x_set_op_mode(BME68X_FORCED_MODE, &bme68x_dev);
    if (ret != BME68X_OK) return -1;
    
    // Wait for measurement (empirical delay for forced mode: ~200ms typical)
    sleep_ms(200);
    
    // Read data
    ret = bme68x_get_data(BME68X_FORCED_MODE, &data, &n_data, &bme68x_dev);
    if (ret != BME68X_OK || n_data == 0) return -1;
    
    // Convert to user-friendly units
    // Note: Bosch API returns values in raw ADC or partially converted form depending on BME68X_USE_FPU
    out->temperature = data.temperature;
    out->pressure = data.pressure / 100.0f;  // Convert Pa to hPa
    out->humidity = data.humidity;
    out->gas_resistance = data.gas_resistance / 1000.0f;  // Convert Ohms to kΩ
    out->gas_valid = (data.status & BME68X_GASM_VALID_MSK) ? true : false;
    out->measurement_index = data.meas_index;
    
    return 0;
}
