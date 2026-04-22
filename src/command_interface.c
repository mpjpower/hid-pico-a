#include "command_interface.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "i2c_interface.h"
#include "uart_interface.h"
#include "led.h"

#define VERSION "1.0.24"

static void skip_spaces(char **p) {
    while (**p == ' ') {
        (*p)++;
    }
}

static int parse_devname(char **p, char *devname, size_t devname_size) {
    size_t n = 0;

    skip_spaces(p);
    while (**p && **p != ' ' && n < devname_size - 1) {
        devname[n++] = **p;
        (*p)++;
    }
    devname[n] = '\0';

    return (n > 0) ? 0 : -1;
}

static int parse_u8_list(char **p, uint8_t *out, int max_count) {
    int count = 0;

    skip_spaces(p);
    if (**p != '{') {
        return -1;
    }
    (*p)++;

    while (1) {
        char *endptr;
        unsigned long value;

        skip_spaces(p);
        value = strtoul(*p, &endptr, 10);
        if (endptr == *p || value > 255 || count >= max_count) {
            return -1;
        }

        out[count++] = (uint8_t) value;
        *p = endptr;

        skip_spaces(p);
        if (**p == ',') {
            (*p)++;
            continue;
        }
        if (**p == '}') {
            (*p)++;
            break;
        }
        return -1;
    }

    return count;
}

void command_interface_process(const uint8_t *buffer, uint16_t bufsize, char *response, size_t response_size) {
    if (!response || response_size == 0) {
        return;
    }

    response[0] = '\0';

    if (!buffer || bufsize == 0) {
        snprintf(response, response_size, "1 Empty command");
        return;
    }

    char command[65];
    memset(command, 0, sizeof(command));

    uint16_t actual_len = 0;
    for (uint16_t i = 0; i < bufsize; i++) {
        if (buffer[i] == '\0') {
            actual_len = i;
            break;
        }
    }
    if (actual_len == 0 && bufsize > 0) {
        actual_len = bufsize;
    }

    if (actual_len >= sizeof(command)) {
        actual_len = sizeof(command) - 1;
    }

    if (actual_len > 0) {
        memcpy(command, buffer, actual_len);
    }
    command[actual_len] = '\0';

    switch (command[0]) {
        case 'V':
            snprintf(response, response_size, "0 Version: %s", VERSION);
            break;

        case 'U': {
            int baud;
            if (sscanf(command + 2, "%d", &baud) == 1) {
                uart_interface_set_baud((uint32_t) baud);
                snprintf(response, response_size, "0 UART set to %d baud", baud);
            } else {
                snprintf(response, response_size, "1 Invalid UART config");
            }
            break;
        }

        case 'S': {
            if (actual_len > 2) {
                int data_len = (int) strlen(command + 2);
                uart_interface_write((const uint8_t *) (command + 2), (size_t) data_len);
                // snprintf(response, response_size, "0 Sent %d bytes to UART", data_len);
            } else {
                // snprintf(response, response_size, "1 No data to send");
            }
            break;
        }

        case 'R': {
            int len = (int) uart_interface_read((uint8_t *) response, response_size - 1);
            response[len] = '\0';
            break;
        }

        case 'L':
            led_on();
            snprintf(response, response_size, "0 LED on");
            break;

        case 'O':
            led_off();
            snprintf(response, response_size, "0 LED off");
            break;

        case 'I': {
            char devname[32] = {0};
            uint8_t i_pairs[32], i_regs[16], i_vals[16];
            char *ip = command + 2;
            int pair_count;
            int i_count;

            if (parse_devname(&ip, devname, sizeof(devname)) != 0) {
                snprintf(response, response_size, "1 Missing I2C device name");
                break;
            }

            pair_count = parse_u8_list(&ip, i_pairs, 32);

            if (pair_count <= 0) {
                snprintf(response, response_size, "1 Invalid format, expected I <dev> {r1,v1,r2,v2,...}");
            } else if ((pair_count % 2) != 0) {
                snprintf(response, response_size, "1 Pairs list must be even length");
            } else {
                i_count = pair_count / 2;
                for (int i = 0; i < i_count; i++) {
                    i_regs[i] = i_pairs[2 * i];
                    i_vals[i] = i_pairs[2 * i + 1];
                }

                if (SetRegs(devname, i_regs, i_vals, i_count) == 0) {
                    snprintf(response, response_size, "0 Set %d reg(s) on %s", i_count, devname);
                } else {
                    if (!i2c_device_known(devname)) {
                        snprintf(response, response_size, "1 Unknown I2C dev %s", devname);
                    } else if (!i2c_device_probe(devname)) {
                        snprintf(response, response_size, "1 No I2C ACK from %s", devname);
                    } else {
                        snprintf(response, response_size, "1 SetRegs failed for %s", devname);
                    }
                }
            }
            break;
        }

        case 'J': {
            char devname[32] = {0};
            uint8_t j_regs[16], j_vals[16];
            char *jp = command + 2;
            int j_count;

            if (parse_devname(&jp, devname, sizeof(devname)) != 0) {
                snprintf(response, response_size, "1 Missing I2C device name");
                break;
            }

            j_count = parse_u8_list(&jp, j_regs, 16);

            if (j_count <= 0) {
                snprintf(response, response_size, "1 Invalid format, expected J <dev> {r1,r2,...}");
            } else if (GetRegs(devname, j_regs, j_count, j_vals) == 0) {
                int off = snprintf(response, response_size, "0 {");
                for (int i = 0; i < j_count && off < (int) response_size - 4; i++) {
                    off += snprintf(response + off,
                                    response_size - (size_t) off,
                                    (i == j_count - 1) ? "%u" : "%u,",
                                    j_vals[i]);
                }
                if (off < (int) response_size - 1) {
                    snprintf(response + off, response_size - (size_t) off, "}");
                }
            } else {
                if (!i2c_device_known(devname)) {
                    snprintf(response, response_size, "1 Unknown I2C dev %s", devname);
                } else if (!i2c_device_probe(devname)) {
                    snprintf(response, response_size, "1 No I2C ACK from %s", devname);
                } else {
                    snprintf(response, response_size, "1 GetRegs failed for %s", devname);
                }
            }
            break;
        }

        default:
            snprintf(response, response_size, "1 Unknown command");
            break;
    }
}
