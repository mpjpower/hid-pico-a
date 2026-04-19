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

Depending on the function, the return value may be a status list, a decoded string, a raw byte list, or `0` on timeout / no response.

- `Open()`  
  Returns `{0,"Pico connected"}` on success or `{1,<error>}` on failure.

- `PicoVersion()`, `LedOn()`, `LedOff()`, `SetUartBaudRate()`  
  Return the decoded firmware response string on success. Return `0` if no response is received.

- `GetRegs()`  
  Returns `{0,{val,val,...}}` on success or `{1,<error>}` on failure.

- `SetRegs()`  
  Returns `{0,<text>}` on success or `{1,<error>}` on failure.

- `Send()` and `SendTimed()`  
  Return `{0,<raw byte list>}` on success, or `0` if no response is received.

- `ReceiveUartBytes()` and `ReceiveUartBytesTimed()`  
  Return a Prime list of byte values. If no UART data is received, they return an empty list `{}`.

- `WaitForMessage(timeout_ms)`  
  Returns a raw byte list on success, or `{}` on timeout.

## Core helpers

### `PicoVersion()`

Returns the firmware version string, e.g. `"0 Version: 1.0.23"`.
Returns `0` if no response is received.

### `LedOn()` / `LedOff()`

Toggles the onboard LED. Returns the firmware acknowledgement string.
Return `0` if no response is received.

### `SetUartBaudRate(baudrate)`

Sets the UART baud rate on the Pico. Example:

```
Pico.SetUartBaudRate(115200)
```

### `SendUart(msg)`

Sends data over UART. Accepts either a string or a Prime byte list (to allow control characters).

## UART receive helpers

### `ReceiveUartBytesTimed(timeout_ms)`

Reads queued UART bytes from the Pico ring buffer, waiting up to `timeout_ms` milliseconds for the response.

Returns a Prime list of byte values up to the first null byte.
Returns an empty list `{}` if no UART data is received before timeout.

### `ReceiveUartBytes()`

Convenience wrapper for `ReceiveUartBytesTimed(10)`.

Reads queued UART bytes from the Pico ring buffer using a default timeout of 10 ms.
Returns a Prime list of byte values up to the first null byte.
Returns an empty list `{}` if no UART data is received.

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

Convenience wrapper for `SendTimed(cmd, 50)`.

Sends a raw command string or byte list to the Pico and waits up to 50 ms for one response.

- Returns `{0,<raw byte list>}` on success
- Returns `0` if no response is received

### `SendTimed(cmd, timeout_ms)`

Sends a raw command string or byte list to the Pico and waits up to `timeout_ms` milliseconds for one response.

- If `cmd` is a string, it is converted to a byte list and a trailing null is appended.
- If `cmd` is already a byte list, only the trailing null is appended.

Returns `{0,<raw byte list>}` on success, or `0` if no response is received before timeout.

### `WaitForMessage(timeout_ms)`

Polls `USBRecv()` until a non-empty response arrives or the timeout expires.

- `timeout_ms` is specified in milliseconds
- negative values are treated as `0`
- the function uses `TICKS` to measure elapsed time
- returns the raw received byte list on success
- returns `{}` on timeout
