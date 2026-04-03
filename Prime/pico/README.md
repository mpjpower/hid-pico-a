# Pico HID API (pico.ppx)

HP PPL library for communicating with the HID Pico A firmware over USB from the HP Prime.

Firmware compatibility: targets Pico firmware version `1.0.20`.

## Source file

`pico.ppx` — copy to the HP Prime, edit and Check to make sure that there are no syntax errors.

## Connecting

```
Pico.Open()
```

Opens the HID connection. Returns `{0,"Pico connected"}` on success or `{1,<error>}`.

USB identifiers: VID `0xCAFE`, PID `0x4001`, report size 64 bytes.

## Response convention

All functions that communicate with the Pico return a two-element list:

- `{0, <payload>}` — success
- `{1, <error text>}` — failure

## Core helpers

### `PicoVersion()`

Returns the firmware version string, e.g. `"0 Version: 1.0.20"`.

### `LedOn()` / `LedOff()`

Toggles the onboard LED. Returns the firmware acknowledgement string.

### `SetUartBaudRate(baudrate)`

Sets the UART baud rate on the Pico. Example:

```
Pico.SetUartBaudRate(115200)
```

### `SendUart(msg)`

Sends data over UART. Accepts either a string or a Prime byte list (to allow control characters).

### `ReceiveUartBytes()`

Reads queued UART bytes from the Pico ring buffer. Returns a Prime list of byte values up to the first null.

## I2C helpers

### `GetRegs(devname, regs)`

Reads one or more I2C registers from the named device.

- `devname` — string device name registered in firmware (e.g. `"tsl2591"`)
- `regs` — Prime list of register numbers, e.g. `{0,1,20,21}`
- Sends wire command: `J <devname> {reg,reg,...}`
- Returns `{0,{val,val,...}}` on success, `{1,<error>}` on failure

Example:

```
Pico.GetRegs("tsl2591", {0,1,20,21})
```

### `SetRegs(devname, pairs)`

Writes one or more I2C registers on the named device.

- `devname` — string device name
- `pairs` — flat Prime list of alternating register/value pairs, e.g. `{0,3,1,16}`
- Sends wire command: `I <devname> {reg,val,reg,val,...}`
- Returns `{0,<success text>}` on success, `{1,<error>}` on failure

Example:

```
Pico.SetRegs("tsl2591", {0,3,1,16})
```

## Low-level transport

### `Send(cmd)`

Sends a raw command string or byte list to the Pico and waits for one response.

- Returns `{0, <raw byte list>}` on success, `0` if no response received.

### `WaitForMessage()`

Polls `USBRecv()` up to 1000 times until a non-empty response arrives.
