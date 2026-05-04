# Leo DCC-EX Throttle

An ESP32-based handheld throttle for [DCC-EX](https://dcc-ex.com/) command stations. Communicates over a direct serial (UART) connection — no WiFi or Bluetooth required.

## Hardware targets

Two display environments are supported via PlatformIO environments:

| Environment | Display | Resolution | Touch |
|-------------|---------|------------|-------|
| `st7796` *(primary)* | ST7796 4" TFT | 480 × 320 | XPT2046 resistive |
| `ili9341` | ILI9341 2.8" TFT | 320 × 240 | none |

All development and testing is done on the ST7796 build. The ILI9341 target exists for the original smaller display.

## Features

- **2 independent throttle slots** — each driven by its own rotary encoder, displayed as a circular gauge with needle
- **Encoder acceleration** — faster turning applies larger speed steps (up to 8×)
- **Direction control** — click the encoder or tap the gauge body to reverse (speed resets to 0)
- **Touchscreen UI** (ST7796) — all navigation and control via the 480×320 resistive touch panel
- **Roster browser** — fetches the loco roster from the command station; tap a row to assign a loco to the active throttle slot
- **Loco persistence** — selected loco address and name survive resets (stored in NVS flash)
- **Loco acquisition** — on roster select the throttle immediately adopts the loco's live speed and direction from the command station
- **Function control** — up to 32 DCC functions per loco, with latching and momentary support
  - Mini strip on the throttle screen shows the first 3 defined functions + a "+" button to open the full function screen
  - Full function screen shows a 4-column scrollable grid; scroll with encoder 0
- **Track power** — PWR button in the top bar toggles track power on/off
- **Track current** — live mA reading shown in the top bar (exponential moving average)
- **Emergency stop** — physical button (GPIO 21) halts all throttles and returns to the throttle screen
- **Display sleep** — backlight fades and display sleeps after 1 minute of inactivity while track power is off; any touch, encoder, or button wakes it
- **Touch calibration** — runs on first boot and stores calibration in NVS; hold the roster button at power-on to force recalibration
- **Splash screen** on boot
- **Test harness** — simulates speed ramps without a command station (see `config.h`)

## Hardware

| Component | Details |
|-----------|---------|
| Microcontroller | ESP32 (38-pin dev board) |
| Display (primary) | 4" 480×320 ST7796 SPI TFT, TFT_CS wired to GND |
| Touch | XPT2046 resistive, shares SPI bus with display |
| Encoders | 2 × rotary encoder with push button |
| Buttons | Emergency stop |
| Connection | UART2 (Serial2) → DCC-EX EX-CommandStation |

### Pin Assignments

| Signal | GPIO | Notes |
|--------|------|-------|
| TFT MOSI | 23 | Shared with touch |
| TFT SCLK | 18 | Shared with touch |
| TFT MISO | 19 | Shared with touch |
| TFT DC | 2 | |
| TFT RST | 4 | |
| TFT CS | GND | Tied low on the display board |
| TFT BL | 5 | PWM backlight (ST7796 only) |
| Touch CS | 13 | |
| Touch IRQ | 36 | Input-only pin |
| DCC-EX TX | 17 | |
| DCC-EX RX | 16 | |
| Encoder 1 CLK / DT / SW | 33 / 14 / 32 | CLK/DT on input-only pins |
| Encoder 2 CLK / DT / SW | 25 / 26 / 27 | |
| Roster button (ILI9341 only) | 22 | Not used in ST7796 build |
| E-stop button | 21 | |

All switch and button pins use internal pull-ups.

## Software

Built with [PlatformIO](https://platformio.org/) on the Arduino framework.

### Dependencies

| Library | Purpose |
|---------|---------|
| [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) | TFT + touch driver |
| [ESP32Encoder](https://github.com/madhephaestus/ESP32Encoder) | Hardware PCNT encoder reading |
| [DCCEXProtocol](https://github.com/DCC-EX/DCCEXProtocol) | DCC-EX serial protocol |

### Architecture

Two FreeRTOS tasks run on separate cores:

- **DCC Task (Core 0)** — owns the serial link to the command station, processes protocol messages, dispatches throttle and function commands
- **UI Task (Core 1)** — drives the display, reads encoder and touch input, manages screen state

Tasks communicate via typed FreeRTOS queues (`DCCEvent` UI←DCC and `UICmd` UI→DCC). The roster and function data are protected by dedicated mutexes. This keeps serial I/O and display rendering fully decoupled.

### Screens (ST7796)

| Screen | How to reach |
|--------|-------------|
| Throttle | Default; back button from any screen |
| Roster | Tap a loco name in the column sub-header |
| Function | Tap "+" in the mini function strip |

Touch zones on the throttle screen:
- **Top bar** (y < 40): PWR and STOP buttons
- **Sub-header** (y 40–76): tap a column to open the roster for that throttle slot
- **Gauge body** (y 76–278): tap to reverse direction (speed → 0)
- **Mini function strip** (y ≥ 278): tap F0–F2 to toggle/momentary; tap "+" for the full function screen

## Getting Started

1. Clone the repository
2. Open in VS Code with the PlatformIO extension installed
3. Adjust pin assignments in [src/config.h](src/config.h) if your wiring differs
4. Flash the ST7796 target:

```
~/.platformio/penv/Scripts/pio run -e st7796 -t upload
```

On first boot, follow the on-screen calibration prompts (tap each corner cross). Calibration data is saved to NVS and reused on subsequent boots. Hold the roster button (GPIO 22) at power-on to force recalibration.

### Test Mode

Enable in [src/config.h](src/config.h) to simulate loco speeds without a command station:

```c
#define ENABLE_TEST_HARNESS  1
```

## Licence

MIT
