#ifndef IR_PRINTER_H
#define IR_PRINTER_H

#include <stddef.h>
#include <stdint.h>

#define IR_PRINTER_GPIO_PIN 6

void ir_printer_init(unsigned int gpio_pin);
void send_ir_frame(uint32_t frame);
void ir_printer_send_byte(uint8_t c);
void ir_printer_send_bytes(const uint8_t *data, size_t len);
void ir_printer_send_carrier_burst_ms(uint32_t duration_ms);
void ir_printer_start_carrier_burst_ms(uint32_t duration_ms);
void ir_printer_task(void);

#endif // IR_PRINTER_H
