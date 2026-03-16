#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "class/hid/hid.h"
#include "class/hid/hid_device.h"
#include "device/usbd.h"

#include "pico/cyw43_arch.h"

static bool cyw43_ok = false;
static bool use_cyw43_led = false;
static int led_pin = 25; // default Pico LED

#define CFG_TUD_HID_EP_BUFSIZE 64

// UART configuration
#define UART_ID uart0
#define BAUD_RATE 115200
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY UART_PARITY_NONE

// UART pins
#define UART_TX_PIN 0
#define UART_RX_PIN 1

// HID report size
#define REPORT_SIZE 64

// Version
#define VERSION "1.0.11"

// Buffer for UART read
char uart_buffer[256];
int uart_buffer_index = 0;

// HID report descriptor for generic in/out
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_GENERIC_INOUT(CFG_TUD_HID_EP_BUFSIZE)
};

// Device descriptors
tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0xCafe,
    .idProduct = 0x4001,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};

// Configuration descriptor
enum {
    ITF_NUM_HID,
    ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + 32)
#define EPNUM_HID 0x01

uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    TUD_HID_INOUT_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), EPNUM_HID, 0x80 | EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 10)
};

// String descriptors
char const* string_desc_arr [] = {
    (const char[]) { 0x09, 0x04 }, // 0: is supported language is English (0x0409)
    "Pico HID Device",             // 1: Manufacturer
    "HID Pico A",                  // 2: Product
    "123456",                      // 3: Serials
};

// Descriptor callbacks
uint8_t const * tud_descriptor_device_cb(void) {
    return (uint8_t const *) &desc_device;
}

uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    (void) index;
    return desc_configuration;
}

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    static uint16_t desc_str[32];
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (index >= sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) return NULL;

        const char* str = string_desc_arr[index];
        chr_count = strlen(str);
        for (uint8_t i = 0; i < chr_count; i++) {
            desc_str[1 + i] = str[i];
        }
    }

    desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);
    return desc_str;
}

uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance) {
    (void) instance;
    return desc_hid_report;
}

// Callback for HID set report (receiving data)
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    if (report_type == HID_REPORT_TYPE_OUTPUT) {
        // Process the command
        char command[REPORT_SIZE + 1];
        memset(command, 0, sizeof(command));  // Clear buffer first
        
        // Find actual data length by looking for null terminator in incoming buffer
        uint16_t actual_len = 0;
        for (uint16_t i = 0; i < bufsize; i++) {
            if (buffer[i] == '\0') {
                actual_len = i;
                break;
            }
        }
        if (actual_len == 0 && bufsize > 0) {
            actual_len = bufsize;  // No null found, use full bufsize
        }
        
        // Copy only the actual data
        if (actual_len > 0) {
            memcpy(command, buffer, actual_len);
        }
        command[actual_len] = '\0';

        // Parse command
        char cmd = command[0];
        char response[REPORT_SIZE] = {0};

        switch (cmd) {
            case 'V':
                snprintf(response, REPORT_SIZE, "Version: %s", VERSION);
                break;
            case 'U':
                // Set UART config, e.g., "U 9600"
                int baud;
                if (sscanf(command + 2, "%d", &baud) == 1) {
                    uart_init(UART_ID, baud);
                    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
                    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
                    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);
                    snprintf(response, REPORT_SIZE, "UART set to %d baud", baud);
                } else {
                    strcpy(response, "Invalid UART config");
                }
                break;
            case 'S':
                // Send over UART
                // Now that buffer is properly cleaned, just find the string length
                if (actual_len > 2) {
                    int data_len = strlen(command + 2);  // Safe now that garbage is removed
                    uart_write_blocking(UART_ID, (uint8_t*)(command + 2), data_len);
                    snprintf(response, REPORT_SIZE, "Sent %d bytes to UART", data_len);
                } else {
                    strcpy(response, "No data to send");
                }
                break;
            case 'R':
                // Read from UART buffer
                int len = 0;
                while (uart_is_readable(UART_ID) && len < REPORT_SIZE - 1) {
                    response[len++] = uart_getc(UART_ID);
                }
                response[len] = '\0';
                if (len == 0) {
                    strcpy(response, "No data in UART buffer");
                }
                break;
            case 'L':
                // Turn on LED
                uart_puts(UART_ID, "Turning LED on\n");
                if (use_cyw43_led && cyw43_ok) {
                    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
                } else {
                    gpio_put(led_pin, 1);
                }
                strcpy(response, "LED on");
                break;
            case 'O':
                // Turn off LED
                uart_puts(UART_ID, "Turning LED off\n");
                if (use_cyw43_led && cyw43_ok) {
                    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
                } else {
                    gpio_put(led_pin, 0);
                }
                strcpy(response, "LED off");
                break;
            default:
                strcpy(response, "Unknown command");
                break;
        }

        // Send response back
        tud_hid_report(0, response, strlen(response) + 1);
    }
}

// Callback for HID get report (sending data)
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    // Not used in this example
    return 0;
}

int main() {
    board_init();

    // Initialize UART
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);

    // Try to initialize Pico W WiFi/LED driver (for onboard LED control)
    if (cyw43_arch_init() == 0) {
        uart_puts(UART_ID, "CYW43 init success\n");
        cyw43_ok = true;
        use_cyw43_led = true;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    } else {
        uart_puts(UART_ID, "CYW43 init failed, using GPIO fallback\n");
        cyw43_ok = false;
        use_cyw43_led = false;
        led_pin = 25;
        gpio_init(led_pin);
        gpio_set_dir(led_pin, GPIO_OUT);
        gpio_put(led_pin, 0);
    }

    // Initialize TinyUSB
    tusb_init();

    while (1) {
        tud_task(); // Handle USB tasks
        // Other tasks if needed
    }

    return 0;
}