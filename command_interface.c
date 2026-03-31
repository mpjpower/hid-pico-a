#include "command_interface.h"

#include <stdio.h>
#include <string.h>

#include "i2c_interface.h"
#include "uart_interface.h"
#include "led.h"

#define VERSION "1.0.19"

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
                snprintf(response, response_size, "0 Sent %d bytes to UART", data_len);
            } else {
                snprintf(response, response_size, "1 No data to send");
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
            uint8_t i_regs[16], i_vals[16];
            int i_count = 0;
            char *ip = command + 2;
            while (*ip == ' ') ip++;
            int ni = 0;
            while (*ip && *ip != ' ' && ni < 31) devname[ni++] = *ip++;
            devname[ni] = '\0';
            while (*ip && i_count < 16) {
                while (*ip == ' ') ip++;
                if (!*ip) break;
                unsigned int r, v;
                if (sscanf(ip, "%u:%u", &r, &v) == 2 && r <= 255 && v <= 255) {
                    i_regs[i_count] = (uint8_t) r;
                    i_vals[i_count] = (uint8_t) v;
                    i_count++;
                }
                while (*ip && *ip != ' ') ip++;
            }
            if (i_count == 0) {
                snprintf(response, response_size, "1 No reg:val pairs");
            } else if (SetRegs(devname, i_regs, i_vals, i_count) == 0) {
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
            break;
        }

        case 'J': {
            char devname[32] = {0};
            uint8_t j_regs[16], j_vals[16];
            int j_count = 0;
            char *jp = command + 2;
            while (*jp == ' ') jp++;
            int nj = 0;
            while (*jp && *jp != ' ' && nj < 31) devname[nj++] = *jp++;
            devname[nj] = '\0';
            while (*jp && j_count < 16) {
                while (*jp == ' ') jp++;
                if (!*jp) break;
                unsigned int r;
                if (sscanf(jp, "%u", &r) == 1 && r <= 255) {
                    j_regs[j_count++] = (uint8_t) r;
                }
                while (*jp && *jp != ' ') jp++;
            }
            if (j_count == 0) {
                snprintf(response, response_size, "1 No registers specified");
            } else if (GetRegs(devname, j_regs, j_count, j_vals) == 0) {
                int off = snprintf(response, response_size, "0 ");
                for (int i = 0; i < j_count && off < (int) response_size - 4; i++) {
                    off += snprintf(response + off, response_size - (size_t) off, "%u ", j_vals[i]);
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
