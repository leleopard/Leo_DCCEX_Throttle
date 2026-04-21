#include "UITask.h"
#include "../config.h"
#include "../SharedTypes.h"
#include "../ui/Display.h"
#include "../input/EncoderManager.h"
#if DISPLAY_480
#include <Preferences.h>
#endif

extern RosterEntry       rosterEntries[MAX_ROSTER_SIZE];
extern int               rosterCount;
extern SemaphoreHandle_t rosterMutex;

enum class Screen { THROTTLE, ROSTER };

static void uiTask(void *param) {
    auto *queues = reinterpret_cast<QueueHandle_t *>(param);
    QueueHandle_t eventQueue = queues[0];
    QueueHandle_t cmdQueue   = queues[1];

    // Check for forced recalibration before splash (hold BTN_ROSTER at power-on)
#if DISPLAY_480
    pinMode(BTN_ROSTER, INPUT_PULLUP);
    delay(50);
    bool forceCalibration = (digitalRead(BTN_ROSTER) == LOW);
#endif

    static Display display;
    display.begin();

    // Load or run touch calibration
#if DISPLAY_480
    {
        Preferences prefs;
        uint16_t calData[5] = {};
        prefs.begin("touch", true);
        bool hasCal = prefs.isKey("cal");
        if (hasCal) prefs.getBytes("cal", calData, sizeof(calData));
        prefs.end();

        if (!hasCal || forceCalibration) {
            display.runCalibration(calData);
            prefs.begin("touch", false);
            prefs.putBytes("cal", calData, sizeof(calData));
            prefs.end();
        }
        display.applyCalibration(calData);
    }
#endif

    static EncoderManager encoders;
    encoders.begin();

    LocoState locoState[NUM_THROTTLES];
    locoState[0].address = LOCO_ADDR_0;
    locoState[1].address = LOCO_ADDR_1;

    Screen   activeScreen    = Screen::THROTTLE;
    bool     connected       = false;
    bool     rosterReady     = false;
    int      rosterScroll    = 0;
    bool     displaySleeping = false;
    uint32_t lastActivityMs  = millis();

#if !DISPLAY_480
    bool rosterBtnPrev = HIGH;
#endif
    bool estopBtnPrev = HIGH;

    display.drawThrottleScreen(locoState, NUM_THROTTLES, connected);

    // Redraw whichever screen is active (called after waking)
    auto redrawActive = [&]() {
        if (activeScreen == Screen::THROTTLE) {
            display.drawThrottleScreen(locoState, NUM_THROTTLES, connected);
        } else {
            if (xSemaphoreTake(rosterMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                display.drawRosterScreen(rosterEntries, rosterCount, rosterScroll);
                xSemaphoreGive(rosterMutex);
            }
        }
    };

    // Call on any user input. Resets the sleep timer.
    // Returns true if the display was sleeping — caller should skip the action
    // so the first interaction just wakes the screen.
    auto noteActivity = [&]() -> bool {
        lastActivityMs = millis();
        if (displaySleeping) {
            displaySleeping = false;
            display.wake();       // SLPOUT + settle; backlight still off
            redrawActive();       // full redraw while backlight is off
            display.fadeInBacklight();
            return true;
        }
        return false;
    };

    DCCEvent evt;
    for (;;) {
        // --- Process incoming DCC events ---
        while (xQueueReceive(eventQueue, &evt, 0) == pdTRUE) {
            switch (evt.type) {
                case DCCEventType::CONNECTED:
                    connected = true;
                    if (!displaySleeping && activeScreen == Screen::THROTTLE)
                        display.drawThrottleScreen(locoState, NUM_THROTTLES, connected);
                    break;

                case DCCEventType::DISCONNECTED:
                    connected = false;
                    if (!displaySleeping && activeScreen == Screen::THROTTLE)
                        display.drawThrottleScreen(locoState, NUM_THROTTLES, connected);
                    break;

                case DCCEventType::ROSTER_READY:
                    rosterReady = true;
                    if (!displaySleeping && activeScreen == Screen::ROSTER) {
                        if (xSemaphoreTake(rosterMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                            display.drawRosterScreen(rosterEntries, rosterCount, rosterScroll);
                            xSemaphoreGive(rosterMutex);
                        }
                    }
                    break;

                case DCCEventType::LOCO_UPDATE:
                    for (int i = 0; i < NUM_THROTTLES; i++) {
                        if (evt.loco.address == locoState[i].address) {
                            bool dirChanged = (evt.loco.forward != locoState[i].forward);
                            locoState[i] = evt.loco;
                            if (!displaySleeping && activeScreen == Screen::THROTTLE) {
                                if (dirChanged)
                                    display.drawThrottleColumn(i, locoState[i], connected);
                                else
                                    display.drawThrottleSpeed(i, locoState[i]);
                            }
                            break;
                        }
                    }
                    break;
            }
        }

        // --- Encoder input (throttle screen only) ---
        if (activeScreen == Screen::THROTTLE) {
            static uint32_t lastEncMs[NUM_THROTTLES] = {};
            for (int i = 0; i < NUM_THROTTLES; i++) {
                int delta = encoders.getDelta(i);
                if (delta != 0) {
                    uint32_t now = millis();
                    uint32_t elapsed = now - lastEncMs[i];
                    lastEncMs[i] = now;
                    int accel = (elapsed < 50) ? 8 : (elapsed < 100) ? 4 : (elapsed < 200) ? 2 : 1;

                    noteActivity();
                    int newSpeed = constrain(locoState[i].speed + delta * accel, 0, 126);
                    if (newSpeed != locoState[i].speed) {
                        locoState[i].speed = newSpeed;
                        display.drawThrottleSpeed(i, locoState[i]);
                        UICmd cmd{ UICmdType::SET_THROTTLE, (uint8_t)i, locoState[i] };
                        xQueueSend(cmdQueue, &cmd, 0);
                    }
                }
                if (encoders.wasClicked(i)) {
                    noteActivity();
                    locoState[i].forward = !locoState[i].forward;
                    locoState[i].speed   = 0;
                    display.drawThrottleSpeed(i, locoState[i]);
                    display.drawGaugeTexts(i, locoState[i]);
                    UICmd cmd{ UICmdType::SET_THROTTLE, (uint8_t)i, locoState[i] };
                    xQueueSend(cmdQueue, &cmd, 0);
                }
            }
        }

        // --- Touch input (ST7796 only) ---
        // Tap throttle screen → toggle direction for tapped column
        // Tap roster screen  → scroll up (upper half) or down (lower half)
        // Any tap while sleeping → wake only, no action
        // Note: roster screen is toggled by the physical BTN_ROSTER button.
        // Touch coordinate calibration is not yet applied — header-zone
        // detection removed until calibration is implemented.
#if DISPLAY_480
        {
            uint16_t tx, ty;
            if (display.getTouch(tx, ty)) {
                bool wasSleeping = noteActivity();
                if (!wasSleeping) {
                    if (activeScreen == Screen::THROTTLE) {
                        int col = tx / (SCREEN_W / NUM_THROTTLES);
                        if (col >= 0 && col < NUM_THROTTLES) {
                            locoState[col].forward = !locoState[col].forward;
                            locoState[col].speed   = 0;
                            display.drawThrottleColumn(col, locoState[col], connected);
                            UICmd cmd{ UICmdType::SET_THROTTLE, (uint8_t)col, locoState[col] };
                            xQueueSend(cmdQueue, &cmd, 0);
                        }
                    } else {
                        // Roster screen — tap upper half scrolls up, lower half scrolls down
                        int newScroll = rosterScroll + (ty < SCREEN_H / 2 ? -1 : 1);
                        newScroll = constrain(newScroll, 0, max(0, rosterCount - Display::ROSTER_VISIBLE_ROWS));
                        if (newScroll != rosterScroll) {
                            rosterScroll = newScroll;
                            if (xSemaphoreTake(rosterMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                                display.drawRosterScreen(rosterEntries, rosterCount, rosterScroll);
                                xSemaphoreGive(rosterMutex);
                            }
                        }
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(300));   // debounce tap
            }
        }
#endif

        // --- Encoder 0: roster scroll ---
        if (activeScreen == Screen::ROSTER) {
            int delta = encoders.getDelta(0);
            if (delta != 0) {
                noteActivity();
                rosterScroll = constrain(rosterScroll + delta, 0,
                                         max(0, rosterCount - Display::ROSTER_VISIBLE_ROWS));
                if (xSemaphoreTake(rosterMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    display.drawRosterScreen(rosterEntries, rosterCount, rosterScroll);
                    xSemaphoreGive(rosterMutex);
                }
            }
        }

        // --- Roster button (physical button, ILI9341 build only) ---
        // In the ST7796 build GPIO 0 is TOUCH_CS — do not read it as a button.
#if !DISPLAY_480
        {
            bool rosterBtnNow = digitalRead(BTN_ROSTER);
            if (rosterBtnPrev == HIGH && rosterBtnNow == LOW) {
                if (!noteActivity()) {
                    if (activeScreen == Screen::THROTTLE) {
                        activeScreen = Screen::ROSTER;
                        rosterScroll = 0;
                        if (xSemaphoreTake(rosterMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                            display.drawRosterScreen(rosterEntries, rosterCount, rosterScroll);
                            xSemaphoreGive(rosterMutex);
                        }
                        if (!rosterReady) {
                            UICmd cmd{ UICmdType::REQUEST_ROSTER, 0, {} };
                            xQueueSend(cmdQueue, &cmd, 0);
                        }
                    } else {
                        activeScreen = Screen::THROTTLE;
                        display.drawThrottleScreen(locoState, NUM_THROTTLES, connected);
                    }
                }
            }
            rosterBtnPrev = rosterBtnNow;
        }
#endif

        // --- E-stop button (always active, wakes display immediately) ---
        {
            bool estopBtnNow = digitalRead(BTN_ESTOP);
            if (estopBtnPrev == HIGH && estopBtnNow == LOW) {
                for (int i = 0; i < NUM_THROTTLES; i++)
                    locoState[i].speed = 0;
                UICmd cmd{ UICmdType::EMERGENCY_STOP, 0, {} };
                xQueueSend(cmdQueue, &cmd, 0);
                lastActivityMs = millis();
                if (displaySleeping) {
                    displaySleeping = false;
                    display.wake();
                }
                if (activeScreen == Screen::THROTTLE)
                    display.drawThrottleScreen(locoState, NUM_THROTTLES, connected);
            }
            estopBtnPrev = estopBtnNow;
        }

        // --- Inactivity sleep ---
#if SLEEP_TIMEOUT_MS > 0
        if (!displaySleeping && (millis() - lastActivityMs) >= SLEEP_TIMEOUT_MS) {
            displaySleeping = true;
            display.sleep();
        }
#endif

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void uiTaskStart(QueueHandle_t eventQueue, QueueHandle_t cmdQueue) {
    static QueueHandle_t queues[2];
    queues[0] = eventQueue;
    queues[1] = cmdQueue;

    xTaskCreatePinnedToCore(
        uiTask, "UI",
        UI_TASK_STACK, queues,
        UI_TASK_PRIORITY, nullptr,
        UI_TASK_CORE
    );
}
