#pragma once

// ---------------------------------------------------------------------------
// DCC-EX serial connection (UART2)
// ---------------------------------------------------------------------------
#define DCCEX_SERIAL    Serial2
#define DCCEX_BAUD      115200
#define DCCEX_RX_PIN    16
#define DCCEX_TX_PIN    17

// ---------------------------------------------------------------------------
// Throttle slots — one per encoder
// ---------------------------------------------------------------------------
#define NUM_THROTTLES        4
#define LOCO_ADDR_0          3
#define LOCO_ADDR_1          4
#define LOCO_ADDR_2          5
#define LOCO_ADDR_3          6
#define DEFAULT_LOCO_ADDRESS LOCO_ADDR_0   // kept for roster highlight

// ---------------------------------------------------------------------------
// Rotary encoders  [CLK, DT, SW]
// Note: CLK/DT can be input-only pins (34/35/36/39); SW needs pull-up support.
// ---------------------------------------------------------------------------
#define ENC1_CLK  34
#define ENC1_DT   35
#define ENC1_SW   32

#define ENC2_CLK  25
#define ENC2_DT   26
#define ENC2_SW   27

#define ENC3_CLK  14
#define ENC3_DT   12
#define ENC3_SW   13

#define ENC4_CLK  21
#define ENC4_DT   22
#define ENC4_SW   33

// ---------------------------------------------------------------------------
// Extra push buttons
// ---------------------------------------------------------------------------
#define BTN_ROSTER   0   // Toggle roster screen (boot button for dev)
#define BTN_ESTOP   15   // Emergency stop

// ---------------------------------------------------------------------------
// Display selection
// Set DISPLAY_480 = 1 for the 4" 480×320 ST7796 display.
// When building via PlatformIO this is set automatically by the environment
// ([env:ili9341] → 0, [env:st7796] → 1).
// ---------------------------------------------------------------------------
#ifndef DISPLAY_480
#define DISPLAY_480  0
#endif

#if DISPLAY_480
#  define SCREEN_W  480
#  define SCREEN_H  320
#else
#  define SCREEN_W  320
#  define SCREEN_H  240
#endif

// ---------------------------------------------------------------------------
// Touch pins (ST7796 / XPT2046 only — ignored when DISPLAY_480 == 0)
// TOUCH_CS is also set in platformio.ini so TFT_eSPI picks it up.
// Touch SPI shares MISO/MOSI/SCLK with the display.
// GPIO 0 is freed from BTN_ROSTER in the ST7796 build (screen toggled by touch).
// ---------------------------------------------------------------------------
#define TOUCH_CS_PIN   0    // T_CS  — also frees BTN_ROSTER for ST7796 build
#define TOUCH_IRQ_PIN  36   // T_IRQ — input-only pin, ideal for interrupt

// ---------------------------------------------------------------------------
// TFT pins (CS not connected — tied low on display board)
// MISO/MOSI/SCLK/DC/RST are set in platformio.ini for TFT_eSPI
// ST7796 BL pin on GPIO 5 — ILI9341 BL tied to 3.3V (no software control)
// ---------------------------------------------------------------------------
#if DISPLAY_480
#  define TFT_BL_PIN   5
#else
#  define TFT_BL_PIN  -1
#endif

// ---------------------------------------------------------------------------
// Display sleep — inactivity timeout before backlight + display IC sleep
// ---------------------------------------------------------------------------
#define SLEEP_TIMEOUT_MS  60000UL   // 1 minute; set 0 to disable

// ---------------------------------------------------------------------------
// Throttle behaviour
// ---------------------------------------------------------------------------
#define SPEED_STEP          1    // Speed change per encoder detent
#define MAX_ROSTER_SIZE    20    // Maximum roster entries to store

// ---------------------------------------------------------------------------
// FreeRTOS task config
// ---------------------------------------------------------------------------
#define DCC_TASK_CORE       0
#define UI_TASK_CORE        1
#define DCC_TASK_PRIORITY   2
#define UI_TASK_PRIORITY    1
#define DCC_TASK_STACK   8192
#define UI_TASK_STACK   32768

// ---------------------------------------------------------------------------
// Test harness — set to 1 to simulate loco speeds instead of real DCC
// ---------------------------------------------------------------------------
#define ENABLE_TEST_HARNESS  1
#define TEST_SPEED_STEP_MS  40   // ms between each speed increment per loco

// Queue depths
#define DCC_EVENT_QUEUE_DEPTH  8
#define UI_CMD_QUEUE_DEPTH     8
