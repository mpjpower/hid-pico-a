#include "led.h"

#include "pico/stdlib.h"
#if PICO_BOARD_HAS_WIRELESS
#include "pico/cyw43_arch.h"
#ifndef CYW43_WL_GPIO_LED_PIN
#define CYW43_WL_GPIO_LED_PIN 0
#endif
#endif

static led_backend_t s_backend = LED_BACKEND_GPIO;
static uint32_t s_gpio_pin = 25;

led_backend_t led_init_auto(uint32_t gpio_pin) {
    s_gpio_pin = gpio_pin;

#if PICO_BOARD_HAS_WIRELESS
    if (cyw43_arch_init() == 0) {
        s_backend = LED_BACKEND_CYW43;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        return s_backend;
    }
#endif

    s_backend = LED_BACKEND_GPIO;
    gpio_init(s_gpio_pin);
    gpio_set_dir(s_gpio_pin, GPIO_OUT);
    gpio_put(s_gpio_pin, 0);
    return s_backend;
}

void led_on(void) {
#if PICO_BOARD_HAS_WIRELESS
    if (s_backend == LED_BACKEND_CYW43) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    } else
#endif
    {
        gpio_put(s_gpio_pin, 1);
    }
}

void led_off(void) {
#if PICO_BOARD_HAS_WIRELESS
    if (s_backend == LED_BACKEND_CYW43) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    } else
#endif
    {
        gpio_put(s_gpio_pin, 0);
    }
}
