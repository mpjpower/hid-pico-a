# Pico HID Python Test

`hid_pico_test.py` is a host-side smoke test for the Pico's USB HID command interface. It checks the firmware version, reads the ADXL343 device ID, enables and reads the TSL2591 light channels, and sends a UART message that can be read back with a TX-to-RX loopback.

## Requirements

- A Pico running this project's firmware, connected to the computer over USB. The script opens the HID device with VID `0xCAFE` and PID `0x4001`.
- The ADXL343 connected on I2C address `0x53`.
- The TSL2591 connected on I2C address `0x29`.
- For the UART echo check, connect GPIO0 (UART TX) to GPIO1 (UART RX). UART is configured for 9600 baud, 8 data bits, no parity, and 1 stop bit.
- Python and the `hidapi` package.

## Install and Run

From the repository root, create a virtual environment and install the HID binding:

```bash
python3 -m venv .venv
.venv/bin/python -m pip install hidapi
```

Run the test:

```bash
.venv/bin/python Python/hid_pico_test.py
```

The script sends HID reports of 64 bytes and waits for each response. It configures the TSL2591 with `I tsl2591 {0, 3, 1, 0}`, waits 500 ms for integration, and reads registers 20-23. For UART, it drains the empty response to the `S` command and waits briefly before polling `R`, so the looped-back text has time to arrive.

## Expected Results

A successful run prints output in this form:

```text
Version: 0 Version: 1.0.61
ADXL343 ID: 0 {229}
TSL2591 setup: 0 Set 2 reg(s) on tsl2591
TSL2591 channels: 0 {123,0,49,0}
UART bytes: b'hello from Python\r\n'
```

The firmware version can differ. The ADXL343 ID should be `229` (`0xE5`), and the TSL2591 setup should return status `0`. The TSL2591 channel values vary with lighting and sensor configuration. They are reported as four bytes in channel order: channel 0 low byte, channel 0 high byte, channel 1 low byte, channel 1 high byte. For example, `{123,0,49,0}` represents channel 0 = 123 and channel 1 = 49.

With GPIO0 connected to GPIO1, the UART result should contain `hello from Python\r\n`. Without a working loopback or other UART source, `UART bytes: b''` is expected. If `import hid` fails, install `hidapi` in the same Python environment used to run the script.
