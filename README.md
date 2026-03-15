# HID Pico A

A Raspberry Pi Pico application that acts as a USB-HID device, responding to ASCII commands over USB.

## Features

- USB-HID interface for communication
- Command parsing: single letter commands with optional parameters
- UART configuration and communication
- Version reporting

## Supported Commands

- `V`: Get version number
- `U <baudrate>`: Set UART baudrate (e.g., `U 9600`)
- `S <text>`: Send text over UART
- `R`: Read from UART buffer and return contents
- `L`: Turn on the LED
- `O`: Turn off the LED
- `P <pin>`: Set the LED GPIO pin (cannot overlap UART pins 0/1; e.g., `P 25` for Pico)

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