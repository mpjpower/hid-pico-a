#ifndef UART_INTERFACE_H
#define UART_INTERFACE_H

#include <stddef.h>
#include <stdint.h>

void uart_interface_init(uint32_t baud_rate);
void uart_interface_set_baud(uint32_t baud_rate);
size_t uart_interface_write(const uint8_t *data, size_t len);
size_t uart_interface_read(uint8_t *out, size_t max_len);
void uart_interface_write_string(const char *text);

#endif // UART_INTERFACE_H
