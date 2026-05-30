#include "command_interface.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "i2c_interface.h"
#include "uart_interface.h"
#include "led.h"
#include "ir_printer.h"
#include "bme68x_pico.h"

#define VERSION "1.0.36"

static const uint8_t IR_SELF_TEST_LINE[] = {
    'H','P','8','2','2','4','0','B',' ','S','E','L','F','-','T','E','S','T',
    ' ','O','K', '\r', '\n'
};

static bool str_ieq(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char) *a) != tolower((unsigned char) *b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static bool parse_positive_int(const char *s, int *value_out) {
    char *endptr;
    long value;

    if (!s || !*s || !value_out) {
        return false;
    }

    value = strtol(s, &endptr, 10);
    if (*endptr != '\0' || value <= 0 || value > 1000000L) {
        return false;
    }

    *value_out = (int) value;
    return true;
}

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

        case 'K': {
            char devname[32] = {0};
            uint8_t k_data[64] = {0};
            char *kp = command + 2;
            int k_count;

            if (parse_devname(&kp, devname, sizeof(devname)) != 0) {
                snprintf(response, response_size, "1 Missing I2C device name");
                break;
            }

            k_count = parse_u8_list(&kp, k_data, (int) sizeof(k_data));

            if (k_count <= 0) {
                snprintf(response, response_size, "1 Invalid format, expected K <dev> {b1,b2,...}");
            } else if (WriteBytes(devname, k_data, k_count) == 0) {
                snprintf(response, response_size, "0 Wrote %d byte(s) to %s", k_count, devname);
            } else {
                if (!i2c_device_known(devname)) {
                    snprintf(response, response_size, "1 Unknown I2C dev %s", devname);
                } else if (!i2c_device_probe(devname)) {
                    snprintf(response, response_size, "1 No I2C ACK from %s", devname);
                } else {
                    snprintf(response, response_size, "1 WriteBytes failed for %s", devname);
                }
            }
            break;
        }

        case 'P': {
            char *pp = command + 1;
            uint8_t p_data[64] = {0};
            int p_len = -1;

            skip_spaces(&pp);
            if (*pp == '{') {
                p_len = parse_u8_list(&pp, p_data, (int) sizeof(p_data));
                if (p_len < 0) {
                    snprintf(response, response_size, "1 Invalid format, expected P text or P {b1,b2,...}");
                    break;
                }

                skip_spaces(&pp);
                if (*pp != '\0') {
                    snprintf(response, response_size, "1 Invalid print data (trailing characters)");
                    break;
                }
            } else if (*pp == '\0') {
                snprintf(response, response_size, "1 No print data");
                break;
            } else {
                size_t plain_len = strlen(pp);
                if (plain_len > sizeof(p_data)) {
                    snprintf(response, response_size, "1 Print data too long (max %u bytes)", (unsigned) sizeof(p_data));
                    break;
                }
                memcpy(p_data, pp, plain_len);
                p_len = (int) plain_len;
            }

            if (p_len == 0) {
                snprintf(response, response_size, "1 No print data");
                break;
            }

            ir_printer_send_bytes(p_data, (size_t) p_len);
            snprintf(response, response_size, "0 Printed %u byte(s)", (unsigned) p_len);
            break;
        }

        case 'T': {
            ir_printer_send_bytes(IR_SELF_TEST_LINE, sizeof(IR_SELF_TEST_LINE));
            snprintf(response, response_size, "0 IR self-test line sent");
            break;
        }

        case 'X': {
            int duration_ms = 2000;
            bool dc_mode = false;
            char arg1[16] = {0};
            char arg2[16] = {0};
            int arg_count = 0;

            if (actual_len > 1) {
                arg_count = sscanf(command + 2, "%15s %15s", arg1, arg2);

                if (arg_count >= 1) {
                    int parsed_value;

                    if (parse_positive_int(arg1, &parsed_value)) {
                        duration_ms = parsed_value;
                        if (arg_count == 2) {
                            if (!str_ieq(arg2, "DC")) {
                                snprintf(response, response_size, "1 Invalid mode, use DC or omit");
                                break;
                            }
                            dc_mode = true;
                        }
                    } else if (str_ieq(arg1, "DC")) {
                        dc_mode = true;
                        duration_ms = 1000;
                        if (arg_count == 2) {
                            if (!parse_positive_int(arg2, &parsed_value)) {
                                snprintf(response, response_size, "1 Invalid DC duration (1..5000 ms)");
                                break;
                            }
                            duration_ms = parsed_value;
                        }
                    } else {
                        snprintf(response, response_size, "1 Invalid X arguments");
                        break;
                    }
                }
            }

            if (dc_mode) {
                if (duration_ms < 1 || duration_ms > 5000) {
                    snprintf(response, response_size, "1 Invalid DC duration (1..5000 ms)");
                    break;
                }
                ir_printer_start_dc_test_ms((uint32_t) duration_ms);
                snprintf(response, response_size, "0 IR DC test started (%d ms)", duration_ms);
                break;
            }

            if (duration_ms < 1 || duration_ms > 10000) {
                snprintf(response, response_size, "1 Invalid burst duration (1..10000 ms)");
                break;
            }

            ir_printer_start_carrier_burst_ms((uint32_t) duration_ms);
            snprintf(response, response_size, "0 IR carrier burst started (%d ms)", duration_ms);
            break;
        }

        case 'N': {
            // N bme68x: Read BME68x sensor data
            char devname[32] = {0};
            char *np = command + 2;
            bme68x_reading_t reading;
            
            if (parse_devname(&np, devname, sizeof(devname)) != 0) {
                snprintf(response, response_size, "1 Missing device name");
                break;
            }
            
            if (!str_ieq(devname, "bme68x")) {
                snprintf(response, response_size, "1 Unknown sensor name");
                break;
            }
            
            if (bme68x_pico_read(&reading) != 0) {
                snprintf(response, response_size, "1 BME68x read failed");
                break;
            }
            
            // Return {temp*100, pressure*100, humidity*100, gas_resistance*100, gas_valid}
            // Using integers to avoid floating-point transmission issues
            int temp_int = (int)(reading.temperature * 100.0f);
            int pres_int = (int)(reading.pressure * 100.0f);
            int hum_int = (int)(reading.humidity * 100.0f);
            int gas_int = (int)(reading.gas_resistance * 100.0f);
            
            snprintf(response, response_size, "0 {%d,%d,%d,%d,%d}", 
                     temp_int, pres_int, hum_int, gas_int, reading.gas_valid ? 1 : 0);
            break;
        }

        case 'M': {
            // M bme68x {gas_en,heater_ms,heater_temp,filter,osr_hum,osr_temp,osr_pres,mode}
            // Configure BME68x sensor
            char devname[32] = {0};
            uint8_t params[8];
            char *mp = command + 2;
            int param_count;
            bme68x_config_t cfg;
            
            if (parse_devname(&mp, devname, sizeof(devname)) != 0) {
                snprintf(response, response_size, "1 Missing device name");
                break;
            }
            
            if (!str_ieq(devname, "bme68x")) {
                snprintf(response, response_size, "1 Unknown sensor name");
                break;
            }
            
            param_count = parse_u8_list(&mp, params, 8);
            
            if (param_count == 0) {
                // No parameters: just acknowledge
                snprintf(response, response_size, "0 BME68x ready");
                break;
            }
            
            if (param_count != 8) {
                snprintf(response, response_size, "1 Expected 8 config params");
                break;
            }
            
            // Parse configuration
            cfg.gas_enabled = params[0];
            cfg.heater_dur = (uint16_t)((params[1] << 8) | params[2]);
            cfg.heater_temp = (uint16_t)((params[3] << 8) | params[4]);
            cfg.filter_coeff = params[5];
            cfg.osr_hum = params[6];
            cfg.osr_temp = params[7];
            cfg.osr_pres = params[7];
            cfg.mode = 1;  // Forced mode
            
            if (bme68x_pico_configure(&cfg) != 0) {
                snprintf(response, response_size, "1 BME68x config failed");
                break;
            }
            
            snprintf(response, response_size, "0 BME68x configured");
            break;
        }

        default:
            snprintf(response, response_size, "1 Unknown command");
            break;
    }
}
