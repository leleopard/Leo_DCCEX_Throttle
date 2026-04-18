# Leo DCC-EX Throttle

An ESP32-based handheld throttle controller for [DCC-EX](https://dcc-ex.com/) model railroad command stations.

## Overview

Leo is a 4-throttle controller that communicates with a DCC-EX EX-CommandStation over a direct serial (UART) connection — no WiFi or Bluetooth required. Four rotary encoders give independent speed and direction control over four locomotives simultaneously, displayed on a 320×240 colour TFT screen.

![Throttle UI](docs/images/throttle_screen.png)

## Features

- **4 independent throttle slots** — each driven by its own rotary encoder
- **Vertical speed bars** with per-slot colour coding (cyan, green, yellow, magenta)
- **Splash screen** on boot
- **Roster browser** — fetches the loco roster from the command station and displays it on screen
- **Emergency stop** — dedicated button halts all throttles instantly
- **DCC-EX connection indicator** per throttle column
- **Test harness** — simulates speed ramps on all 4 throttles without a command station, toggled by a single `#define`

## Hardware

| Component | Details |
|-----------|---------|
| Microcontroller | ESP32 (38-pin dev board) |
| Display | 320×240 TFT LCD, ILI9341 driver, SPI |
| Encoders | 4 × rotary encoder with push button |
| Buttons | Roster toggle, Emergency stop |
| Connection | UART2 (Serial2) → DCC-EX EX-CommandStation |

### Pin Assignments

| Signal | GPIO |
|--------|------|
| TFT MOSI | 23 |
| TFT SCLK | 18 |
| TFT MISO | 19 |
| TFT DC | 2 |
| TFT RST | 4 |
| DCC-EX TX | 17 |
| DCC-EX RX | 16 |
| Encoder 1 CLK/DT/SW | 34 / 35 / 32 |
| Encoder 2 CLK/DT/SW | 25 / 26 / 27 |
| Encoder 3 CLK/DT/SW | 14 / 12 / 13 |
| Encoder 4 CLK/DT/SW | 21 / 22 / 33 |
| Roster button | 0 |
| E-stop button | 15 |

All encoder switch pins and button pins use internal pull-ups.

## Software

Built with [PlatformIO](https://platformio.org/) on the Arduino framework.

### Dependencies

| Library | Purpose |
|---------|---------|
| [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) | TFT display driver |
| [ESP32Encoder](https://github.com/madhephaestus/ESP32Encoder) | Hardware PCNT encoder reading |
| [DCCEXProtocol](https://github.com/DCC-EX/DCCEXProtocol) | DCC-EX serial protocol |

### Architecture

The firmware runs two FreeRTOS tasks on separate cores:

- **DCC Task (Core 0)** — owns the serial link to the command station, processes incoming protocol messages, dispatches throttle commands
- **UI Task (Core 1)** — drives the display and reads encoder/button input

Tasks communicate via typed FreeRTOS queues (`DCCEvent` and `UICmd`), keeping serial I/O and display rendering fully decoupled.

## Getting Started

1. Clone the repository
2. Open in VS Code with the PlatformIO extension installed
3. Adjust pin assignments in `src/config.h` if your wiring differs
4. Set loco addresses in `src/config.h` (`LOCO_ADDR_0` – `LOCO_ADDR_3`)
5. Build and flash: `pio run --target upload`

### Test Mode

To run without a command station or encoders, enable the built-in test harness in `src/config.h`:

```c
#define ENABLE_TEST_HARNESS  1
```

This simulates speed ramps on all 4 throttles, alternating forward and reverse each cycle.

## Project Backlog

Epics and user stories are tracked in [docs/backlog.md](docs/backlog.md).

## Licence

MIT
