# HID Pico A

A Raspberry Pi Pico application that acts as a USB-HID device, responding to ASCII commands over USB.

## Features

- USB-HID interface for communication
- Command parsing: single letter commands with optional parameters
- UART configuration and communication
- I2C device interface with per-device implementation files
- Version reporting

## Supported Commands

- `V`: Get version number
- `U <baudrate>`: Set UART baudrate (e.g., `U 9600`)
- `S <text>`: Send text over UART
- `R`: Read from UART buffer and return contents
- `L`: Turn on the LED
- `O`: Turn off the LED
- `P <pin>`: Set the LED GPIO pin (cannot overlap UART pins 0/1; e.g., `P 25` for Pico)
- `I <device> <reg:val> [reg:val ...]`: Set one or more I2C registers using decimal values 0-255
	- Example: `I tsl2591 0:3 1:30`
- `J <device> <reg> [reg ...]`: Read one or more I2C registers using decimal register numbers 0-255
	- Example: `J tsl2591 20 21`
	- Response format: decimal values only, space-separated, in the same order as requested registers
	- I2C response prefix: `0 ` indicates success, `1 ` indicates error

> Note: On Pico W the onboard LED is driven via the CYW43 WiFi chip, so the `L`/`O` commands use the CYW43 driver to toggle it.

## Building

1. Install the Raspberry Pi Pico SDK and set `PICO_SDK_PATH` environment variable.
2. Create a build directory: `mkdir build && cd build`
3. Configure with CMake: `cmake ..` (for Pico) or `cmake .. -DPICO_BOARD=pico_w` (for Pico W)
4. Build: `make`

## Flashing

Use the generated `hid-pico-a.uf2` file to flash the Pico in BOOTSEL mode.

## Usage

Connect the Pico via USB. Use a host application that can send/receive HID reports to communicate with the device.

For UART, connect pins GPIO0 (TX) and GPIO1 (RX) to your UART device.

For I2C in this firmware, connect:

- GPIO4 as SDA
- GPIO5 as SCL

The current implementation uses `i2c0` at 100 kHz.

## I2C Device Architecture

- Common bus and register API is implemented in `i2c_interface.c` / `i2c_interface.h`.
- Each supported I2C device should be in its own source file.
- Current device support:
	- `lux.c` provides TSL2591 support with device name `tsl2591`.

## Quick I2C Test (TSL2591)

Use these HID command strings (ASCII) to verify I2C access:

1. Enable the sensor (`ENABLE` register 0 = `0x03`):
	- `I tsl2591 0:3`
2. Configure timing/gain (`CONTROL` register 1 = `0x10`, example value):
	- `I tsl2591 1:16`
3. Read back key registers to confirm communication:
	- `J tsl2591 0 1`
4. Read channel data registers (C0DATAL/C0DATAH/C1DATAL/C1DATAH):
	- `J tsl2591 20 21 22 23`

Expected response format for `J` is status prefix + decimal values (space-separated), for example:

- `0 45 0 12 0`

If the sensor does not respond on the bus, commands may return:

- `1 No I2C ACK from tsl2591`

This usually indicates wiring/power/pull-up issues or wrong device address.

## Host Application

To communicate with the device, you need a host program that can send and receive HID reports. You can use Python with the `hid` library:

```python
import hid

# Find the device
device = hid.device()
device.open(0xCafe, 0x4001)  # Vendor ID and Product ID

# Send command
device.write(b'V\x00' + b'\x00' * 61)  # 'V' command padded to 64 bytes

# Read response
response = device.read(64)
print(bytes(response).decode('utf-8').rstrip('\x00'))
```

Note: HID reports are 64 bytes. Commands should be null-terminated and padded.