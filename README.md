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
- `S <text>`: Send raw text bytes over UART. This command does not return a status-prefixed response.
- `R`: Read currently queued UART bytes and return them as a raw HID payload
- `L`: Turn on the LED
- `O`: Turn off the LED
- `I <device> {reg,val,reg,val,...}`: Set one or more I2C registers (decimal values 0-255)
	- Example: `I tsl2591 {0,3,1,16}`
- `J <device> {reg,reg,...}`: Read one or more I2C registers (decimal register numbers 0-255)
	- Example: `J tsl2591 {20,21}`
	- Response format: `0 {val,val,...}` in the same order as requested registers

## Response Format

Most command responses begin with a status prefix:

- `0 ` (ASCII 48 followed by 32): success
- `1 ` (ASCII 49 followed by 32): error

This format is used by commands such as `V`, `U`, `L`, `O`, `I`, and `J`.

Examples:

- `0 Version: 1.0.23`
- `0 {45,0,12,0}`
- `1 No I2C ACK from tsl2591`

UART data commands use a different convention:

- `S <text>` sends UART data and may return an empty HID response
- `R` returns raw queued UART bytes without a status prefix

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

For UART connect the following pins to your UART device:

- GPIO0 TX
- GPIO1 RX
- Suitable GND and 3V3out 

Note: Use a FTDI232 to bring the Pico UART output up to RS232 levels.

For I2C connect:

- GPIO4 as SDA
- GPIO5 as SCL
- Suitable GND and 3V3out

The current implementation uses `i2c0` at 100 kHz.

## I2C Device Architecture

- Common bus and register API is implemented in `i2c_interface.c` / `i2c_interface.h`.
- Each supported I2C device should be in its own source file.
- Current device support:
	- `lux.c` provides TSL2591 support with device name `tsl2591`.

## Quick I2C Test (TSL2591)

Use these HID command strings (ASCII) to verify I2C access:

1. Enable the sensor (`ENABLE` register 0 = `0x03`):
	- `I tsl2591 {0,3}`
2. Configure timing/gain (`CONTROL` register 1 = `0x10`, example value):
	- `I tsl2591 {1,16}`
3. Read back key registers to confirm communication:
	- `J tsl2591 {0,1}`
4. Read channel data registers (C0DATAL/C0DATAH/C1DATAL/C1DATAH):
	- `J tsl2591 {20,21,22,23}`

Expected response format for `J` is status prefix + a comma-separated list in braces, for example:

- `0 {45,0,12,0}`

If the sensor does not respond on the bus, commands may return:

- `1 No I2C ACK from tsl2591`

This usually indicates wiring/power/pull-up issues or wrong device address.

## Host Application

To communicate with the device, you need a host program that can send and receive HID reports. You can use Python with the `hid` library:

```python
import hid

VID = 0xCAFE
PID = 0x4001
REPORT_SIZE = 64

device = hid.device()
device.open(VID, PID)


def write_cmd(data: bytes):
    raw = data + b"\x00"
    if len(raw) > REPORT_SIZE:
        raise ValueError("Command too long for 64-byte HID report")
    device.write(raw + b"\x00" * (REPORT_SIZE - len(raw)))


def read_resp(timeout_ms: int = 200) -> bytes:
    data = bytes(device.read(REPORT_SIZE, timeout_ms))
    nul = data.find(b"\x00")
    return data[:nul] if nul >= 0 else data


def send_status_cmd(cmd: str):
    write_cmd(cmd.encode("ascii"))
    resp = read_resp().decode("ascii", errors="replace")

    if len(resp) < 2 or resp[1] != " " or resp[0] not in ("0", "1"):
        raise RuntimeError(f"Malformed status response: {resp!r}")

    return resp[0], resp[2:]


def uart_send(text: str):
    write_cmd(f"S {text}".encode("utf-8"))


def uart_read() -> bytes:
    write_cmd(b"R")
    return read_resp()


# ---- examples ----

status, payload = send_status_cmd("V")
print("Version:", status, payload)

status, payload = send_status_cmd("J tsl2591 {20,21,22,23}")
print("I2C:", status, payload)

uart_send("hello from Python\r\n")
print("UART bytes:", uart_read())

device.close()

Note: The example above uses separate helpers for status commands (`V`, `U`, `L`, `O`, `I`, `J`) and UART passthrough (`S`, `R`), because `S` and `R` do not use the standard `0 ` / `1 ` status-prefixed response format. HID reports are 64 bytes, and commands are sent as null-terminated, zero-padded payloads.

## UART Buffering

- Incoming UART bytes are captured continuously into a Pico-side ring buffer.
- The RX buffer size is 2048 bytes.
- The `R` command returns currently queued UART data as raw bytes.
- Software flow control is applied automatically on the UART side:
  - XOFF (`0x13`) is sent when the RX buffer reaches the high watermark
  - XON (`0x11`) is sent after the buffer drains back to the low watermark
- This reduces the risk of RX buffer overflow during burst traffic, but overflow is still possible if the sender does not respect flow control or the host drains data too slowly.