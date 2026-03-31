#include <stdbool.h>
#include "pico/stdlib.h"
#include "bsp/board_api.h"
#include "tusb.h"

#include "i2c_interface.h"
#include "lux.h"
#include "uart_interface.h"
#include "hid_transport.h"
#include "led.h"

// UART configuration
#define BAUD_RATE 115200

int main() {
    board_init();
    led_backend_t led_backend;

    // Initialize UART
    uart_interface_init(BAUD_RATE);

    // Initialize LED backend (CYW43 on Pico W, GPIO fallback otherwise).
    led_backend = led_init_auto(25);
    if (led_backend == LED_BACKEND_CYW43) {
        uart_interface_write_string("CYW43 init success\n");
    } else {
        uart_interface_write_string("CYW43 init failed, using GPIO fallback\n");
    }

    // Initialise I2C bus and register devices
    i2c_bus_init();
    lux_register();

    // Initialize TinyUSB
    hid_transport_init();
    tusb_init();

    while (1) {
        tud_task(); // Handle USB tasks
        hid_transport_task();
    }

    return 0;
}