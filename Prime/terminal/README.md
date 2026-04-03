# HP Prime ANSI Terminal (terminal.ppx)

An ANSI-style serial terminal for HP Prime, implemented in HP PPL. Renders a terminal
buffer on screen, parses common ANSI escape sequences, and sends HP Prime keyboard input
to a remote UART device through the Pico serial API (`pico.ppx`).

## Source file

`terminal.ppx`

## Quick start

1. Copy `pico.ppx` and `terminal.ppx` to the Prime, edit them and run Check on each to make sure they compile without syntax errors.
2. Run `ANSI_Terminal()`.
3. Choose **Settings** to configure width, height, font size, and baud rate.
4. Choose **Start Terminal** to begin the terminal loop.

## Current defaults

| Setting    | Value  |
|------------|--------|
| Width      | 26 cols |
| Height     | 15 rows |
| Font size  | 2      |
| Baud rate  | 9600   |

## Main flow

The terminal initialises serial with:

```
Pico.Open()
Pico.SetUartBaudRate(baud_rate)
```

Then enters a loop that:
1. Calls `Pico.ReceiveUartBytes()` and feeds each byte through the ANSI processor.
2. Renders the screen buffer.
3. Reads HP Prime key presses, converts them to bytes/sequences, and calls `Pico.SendUart()`.

## Settings UI

One `INPUT` dialog with four fields:

- Width (cols) — range 1..132
- Height (rows) — range 1..48
- Font size — 1 (small), 2 (medium), 3 (large)
- Baud rate — range 300..115200

## Terminal behaviour

- Per-cell screen buffer stores character, fg colour, bg colour, bold flag.
- Automatic line wrapping.
- Vertical scrolling when cursor moves below the last row.
- 16-colour ANSI palette.
- Off-screen rendering into `G1`, blitted to `G0` with `BLIT_P`.
- Cursor drawn as a white rectangle outline.

## Supported ANSI sequences

| Sequence             | Action                              |
|----------------------|-------------------------------------|
| `ESC [ row ; col H`  | Cursor position                     |
| `ESC [ 2 J`          | Clear screen                        |
| `ESC [ K`            | Clear to end of line                |
| `ESC [ A/B/C/D`      | Cursor up / down / right / left     |
| `ESC [ P`            | Delete character at cursor          |
| `ESC [ n P`          | Delete n characters at cursor       |
| `ESC [ … m`          | SGR — reset, bold, 16-colour fg/bg  |

SGR codes handled: `0`, `1`, `30–37`, `40–47`, `90–97`, `100–107`.

## Control characters

| Byte       | Action                                |
|------------|---------------------------------------|
| LF (10)    | Move to next line                     |
| CR (13)    | Carriage return (column 0)            |
| BS (8)     | Backspace (move cursor left)          |
| DEL (127)  | Delete at cursor (shift line left)    |

## HP Prime keyboard mapping

### Core control keys

| Key                | Sends               |
|--------------------|---------------------|
| Menu (id 13)       | Exit terminal       |
| Esc (id 4)         | ASCII ESC (27)      |
| Enter (id 30)      | CR (13)             |
| Shift + Enter      | LF (10)             |
| Shift + Del        | BS (8)              |
| Del (id 19)        | DEL (127)           |
| Shift + .          | `=` (61)            |
| Shift + Space      | `_` (95)            |
| Shift + +          | `;` (59)            |
| Shift + -          | `:` (58)            |
| Shift + 0          | `"` (34)            |
| Alpha + 0          | `"` (34)            |

### Arrow keys → ANSI sequences

| Key         | Sequence        |
|-------------|-----------------|
| Up (id 2)   | `ESC [ A`       |
| Down (id 12)| `ESC [ B`       |
| Right (id 8)| `ESC [ C`       |
| Left (id 7) | `ESC [ D`       |

### Alpha and shift states

- **Alpha key (id 36):** toggles alpha lock.
- **Shift key (id 41):** single press = one-shot shift; double press = toggle shift lock.

In alpha mode keys map to lowercase `a–z`, or uppercase with shift active.

### On-screen indicators (top-right)

| Text | Meaning                         |
|------|---------------------------------|
| `a`  | Alpha lock, lowercase           |
| `A`  | Alpha lock, uppercase           |
| `Sh` | One-shot shift armed            |
| `SL` | Shift lock active               |

### Alpha key map

```
A=14  B=15  C=16  D=17  E=18
F=20  G=21  H=22  I=23  J=24  K=25  L=26  M=27  N=28  O=29
P=31  Q=32  R=33  S=34  T=35
U=37  V=38  W=39  X=40
Y=42  Z=43  #=44
```

### Numeric mode map

| Keys (id)       | Characters  |
|-----------------|-------------|
| 32 33 34 35     | 7 8 9 /     |
| 37 38 39 40     | 4 5 6 *     |
| 42 43 44 45     | 1 2 3 -     |
| 47 48 49 50     | 0 . space + |
