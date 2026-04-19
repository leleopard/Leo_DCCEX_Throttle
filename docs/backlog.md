# Leo DCC Throttle — Epics & User Stories

**Project:** ESP32-based 4-throttle DCC-EX controller  
**Display:** 320×240 TFT LCD (ILI9341, landscape)  
**Hardware:** 4 clickable rotary encoders, 2 push buttons, ESP32

**Status legend:** ✅ Done · 🚧 In Progress · 📋 To Do

---

## Epic 1 — Platform & Infrastructure

> Set up the development environment, hardware targets, and core runtime architecture.

| ID | User Story | Status |
|----|------------|--------|
| 1.1 | As a developer, I want a PlatformIO project targeting ESP32 Arduino so that I can build and flash firmware from VS Code. | ✅ |
| 1.2 | As a developer, I want TFT_eSPI configured for the ILI9341 display so that graphics render correctly without manual SPI setup. | ✅ |
| 1.3 | As a developer, I want a dual-core FreeRTOS architecture (DCC on Core 0, UI on Core 1) so that serial communication never blocks the display. | ✅ |
| 1.4 | As a developer, I want typed inter-task queues (`DCCEvent`, `UICmd`) so that tasks communicate safely without shared mutable state. | ✅ |
| 1.5 | As a developer, I want all pin assignments and tunable constants in `config.h` so that hardware changes require edits in one place only. | ✅ |
| 1.6 | As a developer, I want the project in version control so that changes are tracked and reversible. | ✅ |
| 1.7 | As a developer, I want a top-level `DISPLAY_480` compile flag so that the firmware can target either the 320×240 ILI9341 or the 480×320 ST7796 display without code changes. | ✅ |
| 1.8 | As an operator, I want the display to sleep after 1 minute of inactivity so that screen burn-in is prevented and power is saved. | ✅ |
| 1.9 | As a developer, I want touch input on the ST7796 to toggle loco direction by tapping a throttle column so that the screen can be used as an alternative to encoder clicks. | ✅ |

---

## Epic 2 — Display & UI

> Everything the operator sees on screen.

| ID | User Story | Status |
|----|------------|--------|
| 2.1 | As an operator, I want a splash screen on boot so that the device feels polished when powering on. | ✅ |
| 2.2 | As an operator, I want the display in landscape orientation so that 4 throttle columns fit side by side. | ✅ |
| 2.3 | As an operator, I want 4 throttle columns displayed simultaneously so that I can monitor all locos at a glance. | ✅ |
| 2.4 | As an operator, I want a vertical speed bar per throttle so that speed is immediately readable without looking at a number. | ✅ |
| 2.5 | As an operator, I want each throttle bar in a distinct colour so that I can identify throttle slots at a glance. | ✅ |
| 2.6 | As an operator, I want smooth proportional fonts so that the display looks professional rather than blocky. | ✅ |
| 2.7 | As an operator, I want flicker-free speed bar updates so that rapid encoder turns don't cause visual noise. | ✅ |
| 2.8 | As an operator, I want a connection status indicator per throttle column so that I know immediately if DCC-EX is offline. | ✅ |
| 2.9 | As an operator, I want the loco address displayed in each column header so that I know which loco each throttle controls. | ✅ |
| 2.10 | As an operator, I want loco names displayed in the column header (from roster) so that I recognise locos by name not just address. | 📋 |
| 2.11 | As an operator, I want a dedicated roster browse screen so that I can see all locos registered on the command station. | ✅ |

---

## Epic 3 — Throttle Control

> Physical input handling and loco command dispatch.

| ID | User Story | Status |
|----|------------|--------|
| 3.1 | As an operator, I want to rotate encoder N to control the speed of throttle slot N so that I can drive 4 locos independently. | ✅ |
| 3.2 | As an operator, I want to click encoder N to toggle the direction of throttle slot N so that I can reverse without extra buttons. | ✅ |
| 3.3 | As an operator, I want direction to reset speed to zero on toggle so that locos don't jump direction at speed. | ✅ |
| 3.4 | As an operator, I want a dedicated e-stop button that halts all throttles instantly so that I can react quickly to an emergency. | ✅ |
| 3.5 | As an operator, I want the roster button to toggle between the throttle screen and the roster screen so that I can browse locos without losing throttle state. | ✅ |
| 3.6 | As an operator, I want to select a loco from the roster and assign it to a throttle slot so that I don't need to hardcode addresses in firmware. | 📋 |
| 3.7 | As an operator, I want momentum/inertia simulation per throttle so that acceleration and braking feel prototypical. | 📋 |

---

## Epic 4 — DCC-EX Communication

> Serial protocol integration with the DCC-EX command station.

| ID | User Story | Status |
|----|------------|--------|
| 4.1 | As a developer, I want the DCCEXProtocol library integrated so that I don't implement the wire protocol manually. | ✅ |
| 4.2 | As a developer, I want the throttle to detect when the command station connects and disconnects so that the UI reflects actual link state. | ✅ |
| 4.3 | As an operator, I want speed and direction commands sent to the command station in real time so that loco movement matches the encoder position. | ✅ |
| 4.4 | As an operator, I want emergency stop forwarded to the command station so that track power is cut immediately. | ✅ |
| 4.5 | As a developer, I want the throttle to request and receive the roster from the command station so that loco names are available without manual entry. | ✅ |
| 4.6 | As a developer, I want automatic reconnection if the serial link drops so that a command station restart doesn't require a throttle reboot. | 📋 |
| 4.7 | As an operator, I want loco broadcast updates from the command station reflected on screen so that the display stays in sync if another throttle moves the same loco. | ✅ |

---

## Epic 5 — Function Keys

> Controlling DCC function outputs (lights, sounds, etc.).

| ID | User Story | Status |
|----|------------|--------|
| 5.1 | As an operator, I want to toggle F0 (headlight) per throttle slot so that I can control the most common function quickly. | 📋 |
| 5.2 | As an operator, I want a function key UI accessible from each throttle column so that I can trigger F1–F8 without leaving the throttle screen. | 📋 |
| 5.3 | As an operator, I want active function states shown on screen so that I know which functions are currently on. | 📋 |

---

## Epic 6 — Consist Control

> Managing multiple locos running together as a single unit.

| ID | User Story | Status |
|----|------------|--------|
| 6.1 | As an operator, I want to add a loco to a consist from the throttle so that I can run multiple units on one encoder. | 📋 |
| 6.2 | As an operator, I want to remove a loco from a consist so that I can split units after a run. | 📋 |
| 6.3 | As an operator, I want the consist shown on the throttle column display so that I know how many units are linked. | 📋 |

---

## Epic 7 — Testing & Quality

> Tools and infrastructure for validating behaviour without live hardware.

| ID | User Story | Status |
|----|------------|--------|
| 7.1 | As a developer, I want a test harness that simulates speed ramps on all 4 throttles so that I can validate the UI without a command station or encoders. | ✅ |
| 7.2 | As a developer, I want the test harness to alternate forward and reverse each cycle so that direction-change rendering is exercised. | ✅ |
| 7.3 | As a developer, I want test mode toggled by a single `#define` in `config.h` so that I never accidentally ship test code. | ✅ |
| 7.4 | As a developer, I want structured serial debug logging so that I can diagnose DCC protocol issues without a hardware debugger. | 📋 |
