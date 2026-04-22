#ifndef LED_H
#define LED_H

#include <stdint.h>

typedef enum {
    LED_BACKEND_GPIO = 0,
    LED_BACKEND_CYW43 = 1
} led_backend_t;

led_backend_t led_init_auto(uint32_t gpio_pin);
void led_on(void);
void led_off(void);

#endif // LED_H
