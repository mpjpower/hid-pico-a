#include <stdbool.h>
#include "pico/stdlib.h"
#include "bsp/board_api.h"
#include "tusb.h"

#include "adxl343.h"
#include "bme680.h"
#include "bme688.h"
#include "i2c_interface.h"
#include "lux.h"
#include "uart_interface.h"
#include "hid_transport.h"
#include "led.h"
#include "ir_printer.h"

// UART configuration
#define BAUD_RATE 9600
#define UART_START_DELAY_MS 250

int main() {
    board_init();
    led_backend_t led_backend;

    // Initialize UART
    sleep_ms(UART_START_DELAY_MS);
    uart_interface_init(BAUD_RATE);

    // Initialize LED backend (CYW43 on Pico W, GPIO fallback otherwise).
    led_backend = led_init_auto(25);

    // Reserve GPIO6 for IR printer output (avoids UART GPIO0/1 and I2C GPIO4/5).
    ir_printer_init(IR_PRINTER_GPIO_PIN);

    // Initialise I2C bus and register devices
    i2c_bus_init();
    adxl343_register();
    bme680_register();
    bme688_register();
    lux_register();

    // Initialize TinyUSB
    hid_transport_init();
    tusb_init();

    while (1) {
        tud_task(); // Handle USB tasks
        ir_printer_task();
        hid_transport_task();
    }

    return 0;
}