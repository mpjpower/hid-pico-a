#include "uart_interface.h"

#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/uart.h"

// UART configuration
#define UART_ID uart0
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY UART_PARITY_NONE

// UART pins
#define UART_TX_PIN 0
#define UART_RX_PIN 1

// UART RX ring buffer size. Must be a power of two.
#define UART_RX_BUFFER_SIZE 1024

static volatile uint16_t uart_rx_head = 0;
static volatile uint16_t uart_rx_tail = 0;
static volatile uint32_t uart_rx_overflow_count = 0;
static uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];

static inline bool uart_rx_buffer_is_empty(void) {
    return uart_rx_head == uart_rx_tail;
}

static inline void uart_rx_buffer_push(uint8_t ch) {
    uint16_t next_head = (uart_rx_head + 1u) & (UART_RX_BUFFER_SIZE - 1u);

    if (next_head == uart_rx_tail) {
        uart_rx_overflow_count++;
        return;
    }

    uart_rx_buffer[uart_rx_head] = ch;
    uart_rx_head = next_head;
}

static inline int uart_rx_buffer_pop(uint8_t *ch) {
    if (uart_rx_buffer_is_empty()) {
        return 0;
    }

    *ch = uart_rx_buffer[uart_rx_tail];
    uart_rx_tail = (uart_rx_tail + 1u) & (UART_RX_BUFFER_SIZE - 1u);
    return 1;
}

static void on_uart_rx(void) {
    while (uart_is_readable(UART_ID)) {
        uart_rx_buffer_push((uint8_t) uart_getc(UART_ID));
    }
}

void uart_interface_init(uint32_t baud_rate) {
    uart_init(UART_ID, baud_rate);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);
    uart_set_fifo_enabled(UART_ID, true);
    irq_set_exclusive_handler(UART0_IRQ, on_uart_rx);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(UART_ID, true, false);
}

void uart_interface_set_baud(uint32_t baud_rate) {
    uart_interface_init(baud_rate);
}

size_t uart_interface_write(const uint8_t *data, size_t len) {
    if (!data || len == 0) {
        return 0;
    }

    uart_write_blocking(UART_ID, data, len);
    return len;
}

size_t uart_interface_read(uint8_t *out, size_t max_len) {
    if (!out || max_len == 0) {
        return 0;
    }

    size_t len = 0;
    uint32_t irq_state = save_and_disable_interrupts();
    while (len < max_len && uart_rx_buffer_pop(&out[len])) {
        len++;
    }
    restore_interrupts(irq_state);
    return len;
}

void uart_interface_write_string(const char *text) {
    if (!text) {
        return;
    }

    uart_puts(UART_ID, text);
}
