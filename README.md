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
- `R`: Read the next queued UART bytes and return one HID-sized chunk
- `L`: Turn on the LED
- `O`: Turn off the LED
- `I <device> {reg,val,reg,val,...}`: Set one or more I2C registers (decimal values 0-255)
	- Example: `I tsl2591 {0,3,1,16}`
- `J <device> {reg,reg,...}`: Read one or more I2C registers (decimal register numbers 0-255)
	- Example: `J tsl2591 {20,21}`
	- Response format: `0 {val,val,...}` in the same order as requested registers

## Response Format

All command responses begin with a status prefix:

- `0 ` (ASCII 48 then 32): success
- `1 ` (ASCII 49 then 32): error

Examples:

- `0 Version: 1.0.20`
- `0 {45,0,12,0}`
- `1 No I2C ACK from tsl2591`

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

device = hid.device()
device.open(0xCafe, 0x4001)  # Vendor ID and Product ID

def send_cmd(cmd: str):
	# HID OUT report must be 64 bytes; command is null-terminated then padded.
	raw = cmd.encode("ascii") + b"\x00"
	if len(raw) > 64:
		raise ValueError("Command too long for 64-byte HID report")
	device.write(raw + b"\x00" * (64 - len(raw)))

	resp = bytes(device.read(64)).decode("ascii", errors="replace").rstrip("\x00").strip()
	if len(resp) < 2 or resp[1] != " ":
		raise RuntimeError(f"Malformed response: {resp!r}")

	status = resp[0]    # '0' success, '1' error
	payload = resp[2:]  # message/data after status prefix
	return status, payload

# Example: version query
status, payload = send_cmd("V")
print(f"status={status} payload={payload}")

# Example: I2C read
status, payload = send_cmd("J tsl2591 {20,21,22,23}")
if status == "0":
	values = [int(x) for x in payload.strip("{}").split(",") if x]
	print("I2C values:", values)
else:
	print("I2C error:", payload)
```

Note: HID reports are 64 bytes. Commands should be null-terminated and padded.

## UART Buffering

- Incoming UART bytes are now captured continuously into a Pico-side ring buffer.
- The `R` command returns queued UART data in batches of up to 63 bytes per HID response.
- This reduces dropped UART data when the remote system sends bursts faster than the host polls HID.
- Sustained traffic can still overflow the Pico-side queue if the host drains it too slowly. If the remote computer supports it, flow control such as RTS/CTS or XON/XOFF is still useful for long bursts, but would need adjustments to the code on the Pico.