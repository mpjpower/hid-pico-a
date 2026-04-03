# HP Prime ANSI Terminal

## Overview

This project is an ANSI-style serial terminal for HP Prime, implemented in HP PPL.
It renders a terminal buffer on screen, parses common ANSI escape sequences, and sends HP Prime keyboard input to a remote UART device through the Pico serial API.

Firmware compatibility: this Prime API/doc set targets Pico firmware version `1.0.20`.

## Current Defaults

- Width: 26
- Height: 15
- Font size: 2
- Baud rate: 9600

These are defined in terminal.ppx as exported settings variables.

## Main Flow

1. Run ANSI_Terminal().
2. Choose Start Terminal to launch the terminal loop.
3. Choose Settings to edit width, height, font size, and baud rate on a single settings page.

The terminal initializes serial with:

- Pico.Open()
- Pico.SetUartBaudRate(baud_rate)

## Settings UI

Settings are edited in one form page (single INPUT dialog) with four fields:

- Width (cols)
- Height (rows)
- Font Size
- Baud Rate

Entered values are clamped to valid ranges:

- Width: 1..132
- Height: 1..48
- Font size: 1..3
- Baud: 300..115200

## Implemented Terminal Behavior

- Screen buffer stores per-cell state: character, fg, bg, bold.
- Automatic line wrapping is enabled.
- Vertical scrolling is implemented when cursor moves below the last row.
- 16-color ANSI palette is supported.
- Rendering uses off-screen buffer G1 and then BLIT_P(G0, G1).
- Character cell dimensions are calculated during `init_terminal()` and reused during rendering.
- Cursor is drawn as a white rectangle outline.

## Supported ANSI Sequences

- ESC [ row ; col H  : cursor position
- ESC [ 2J           : clear screen
- ESC [ K            : clear line
- ESC [ A/B/C/D      : cursor up/down/right/left
- ESC [ P            : delete character at cursor (line shifts left)
- ESC [ n P          : delete n characters at cursor
- ESC [ m            : SGR handling for reset, bold, basic and bright fg/bg colors

SGR handled codes:

- 0, 1
- 30..37, 40..47
- 90..97, 100..107

## Control Characters

Incoming bytes:

- LF (10): move to next line
- CR (13): carriage return (column 0)
- BS (8): backspace (move cursor left, no deletion)
- DEL (127): delete at cursor (shift left)

## HP Prime Keyboard Output Mapping

### Core control keys

- Menu key (key id 13): exits terminal
- Esc key (key id 4): sends ASCII ESC (27)
- Enter key (key id 30): sends CR (ASCII 13)
- Shift + Enter: sends LF (ASCII 10)
- Shift + .: sends = (ASCII 61)
- Shift + Space: sends _ (ASCII 95)
- Shift + +: sends ; (ASCII 59)
- Shift + -: sends : (ASCII 58)
- Shift + 0: sends " (ASCII 34)
- Alpha + 0: sends " (ASCII 34)
- Arrow keys send ANSI cursor sequences:
  - Up key id 2: ESC [ A
  - Down key id 12: ESC [ B
  - Right key id 8: ESC [ C
  - Left key id 7: ESC [ D

### Delete and Backspace

- Del key (key id 19): sends DEL (ASCII 127)
- Shift + Del: sends BS (ASCII 8)

### Alpha and Shift states

- Alpha key (key id 36): toggles alpha lock on/off.
- Shift key (key id 41):
  - Single press: one-shot shift.
  - Double press: toggles shift lock on/off.

### On-screen state indicators

Top-right indicator text in render view:

- a   : alpha lock active (lowercase mode)
- A   : alpha lock active with uppercase (shift or shift lock)
- Sh  : one-shot shift armed
- SL  : shift lock active

Combinations are displayed together when relevant (for example A SL).

### Alpha key map (definitive)

- A=14, B=15, C=16, D=17, E=18
- F=20, G=21, H=22, I=23, J=24, K=25, L=26, M=27, N=28, O=29
- P=31, Q=32, R=33, S=34, T=35
- U=37, V=38, W=39, X=40
- Y=42, Z=43, #=44

In alpha mode these map to ASCII letters a-z, or A-Z when shift is active.

### Numeric mode map

When alpha is off, supported key ids send:

- 7 8 9 / from key ids 32 33 34 35
- 4 5 6 * from key ids 37 38 39 40
- 1 2 3 - from key ids 42 43 44 45
- 0 . space + from key ids 47 48 49 50

## Files

- terminal.ppx: Main HP PPL terminal program
- pico.ppx: Pico HID API wrapper (Open, UART, LED, I2C helpers)
- README.md: This documentation

## Pico API (pico.ppx)

The terminal uses helpers in `pico.ppx` to talk to the Pico HID firmware.

### Core helpers

- `Open()`
- `PicoVersion()`
- `LedOn()` / `LedOff()`
- `SetUartBaudRate(baudrate)`
- `SendUart(msg)`
- `ReceiveUartBytes()`

### I2C helpers

- `GetRegs(devname, regs)`
  - `regs` is a Prime list of register numbers, e.g. `{0,1,20,21}`
  - Sends command format: `J <devname> {reg,reg,...}`
  - On success returns `{0,{val,val,...}}`
  - On failure returns `{1,<error text>}`

- `SetRegs(devname, pairs)`
  - `pairs` is a single flat Prime list of register/value pairs, e.g. `{0,3,1,16}`
  - Sends command format: `I <devname> {reg,val,reg,val,...}`
  - On success returns `{0,<success text>}`
  - On failure returns `{1,<error text>}`

Examples:

- `SetRegs("tsl2591", {0,3,1,16})`
- `GetRegs("tsl2591", {0,1,20,21})`
