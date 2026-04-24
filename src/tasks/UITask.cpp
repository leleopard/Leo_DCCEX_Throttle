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
    char locoName[NUM_THROTTLES][32] = {};

#if DISPLAY_480
    // Load persisted loco selection (Option C: show name immediately, refresh from roster later)
    {
        Preferences prefs;
        prefs.begin("locos", true);
        for (int i = 0; i < NUM_THROTTLES; i++) {
            char addrKey[6], nameKey[6];
            snprintf(addrKey, sizeof(addrKey), "addr%d", i);
            snprintf(nameKey, sizeof(nameKey), "name%d", i);
            locoState[i].address = prefs.getInt(addrKey, locoState[i].address);
            prefs.getString(nameKey, locoName[i], sizeof(locoName[0]));
        }
        prefs.end();
    }
#endif
    uint32_t lastLocalInputMs[NUM_THROTTLES] = {};

    Screen   activeScreen      = Screen::THROTTLE;
    bool     connected         = false;
    bool     trackPower        = false;
    bool     rosterReady       = false;
    int      rosterScroll      = 0;
    int      selectedThrottle  = 0;   // which slot opened the roster
    int      currentMa         = -1;
    bool     displaySleeping   = false;
    uint32_t lastActivityMs    = millis();

#if !DISPLAY_480
    bool rosterBtnPrev = HIGH;
#endif
    bool estopBtnPrev = HIGH;

    display.drawThrottleScreen(locoState, NUM_THROTTLES, connected, trackPower, currentMa, locoName);

    // Redraw whichever screen is active (called after waking)
    auto redrawActive = [&]() {
        if (activeScreen == Screen::THROTTLE) {
            display.drawThrottleScreen(locoState, NUM_THROTTLES, connected, trackPower, currentMa, locoName);
        } else {
            if (xSemaphoreTake(rosterMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                display.drawRosterScreen(rosterEntries, rosterCount, rosterScroll,
                                         locoState[selectedThrottle].address, rosterReady);
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
            display.wake();       // SLPOUT + settle + fillScreen(BLACK)
            redrawActive();       // draw after settle, backlight still off
            display.fadeInBacklight();
            return true;
        }
        return false;
    };

    DCCEvent evt;
    uint32_t loopMaxMs  = 0;
    uint32_t lastDiagMs = 0;
    for (;;) {
        uint32_t loopStart = millis();

        // --- Process incoming DCC events ---
        while (xQueueReceive(eventQueue, &evt, 0) == pdTRUE) {
            switch (evt.type) {
                case DCCEventType::CONNECTED:
                    if (!connected) {
                        connected = true;
                        if (!displaySleeping && activeScreen == Screen::THROTTLE)
                            display.drawColHeaders(connected, locoState, NUM_THROTTLES, locoName);
                    }
                    break;

                case DCCEventType::DISCONNECTED:
                    if (connected) {
                        connected = false;
                        trackPower = false;
                        if (!displaySleeping && activeScreen == Screen::THROTTLE) {
                            display.drawTopBar(trackPower);
                            display.drawColHeaders(connected, locoState, NUM_THROTTLES, locoName);
                        }
                    }
                    break;

                case DCCEventType::TRACK_POWER_ON:
                    if (!trackPower) {
                        trackPower = true;
                        noteActivity();
                        if (!displaySleeping && activeScreen == Screen::THROTTLE)
                            display.drawTopBar(trackPower);
                    }
                    break;

                case DCCEventType::TRACK_POWER_OFF:
                    if (trackPower) {
                        trackPower = false;
                        if (!displaySleeping && activeScreen == Screen::THROTTLE)
                            display.drawTopBar(trackPower);
                    }
                    break;

                case DCCEventType::CURRENT_UPDATE: {
                    // Exponential moving average (alpha≈0.1): 90% old, 10% new
                    int prev = currentMa;
                    if (currentMa < 0)
                        currentMa = evt.value;
                    else
                        currentMa = (currentMa * 9 + evt.value) / 10;
                    if (currentMa != prev && !displaySleeping && activeScreen == Screen::THROTTLE)
                        display.drawCurrentReading(currentMa);
                    break;
                }

                case DCCEventType::ROSTER_READY:
                    rosterReady = true;
                    // Refresh names for all throttle slots from roster
                    if (xSemaphoreTake(rosterMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                        for (int i = 0; i < NUM_THROTTLES; i++) {
                            for (int j = 0; j < rosterCount; j++) {
                                if (rosterEntries[j].address == locoState[i].address) {
                                    strncpy(locoName[i], rosterEntries[j].name,
                                            sizeof(locoName[0]) - 1);
                                    break;
                                }
                            }
                        }
                        if (!displaySleeping) {
                            if (activeScreen == Screen::ROSTER) {
                                display.drawRosterScreen(rosterEntries, rosterCount, rosterScroll,
                                             locoState[selectedThrottle].address, rosterReady);
                            } else {
                                display.drawColHeaders(connected, locoState, NUM_THROTTLES, locoName);
                            }
                        }
                        xSemaphoreGive(rosterMutex);
                    }
                    break;

                case DCCEventType::LOCO_UPDATE:
                    connected  = true;   // infer from live updates
                    trackPower = true;   // updates only arrive when power is on
                    lastActivityMs = millis();
                    for (int i = 0; i < NUM_THROTTLES; i++) {
                        if (evt.loco.address == locoState[i].address) {
                            // Suppress CS echo for 300ms after local input.
                            // The CS echoes the old speed before processing our command,
                            // which would bounce the needle back and forth.
                            if (millis() - lastLocalInputMs[i] < 300) break;
                            if (evt.loco.speed    != locoState[i].speed ||
                                evt.loco.forward  != locoState[i].forward) {
                                locoState[i] = evt.loco;
                                if (!displaySleeping && activeScreen == Screen::THROTTLE)
                                    display.drawThrottleSpeed(i, locoState[i]);
                            }
                            break;
                        }
                    }
                    break;
            }
        }

        // --- Encoder input ---
        // On roster screen: encoder 0 scrolls the list.
        // On throttle screen: all encoders control speed; click reverses direction.
        {
            static uint32_t lastEncMs[NUM_THROTTLES] = {};
            for (int i = 0; i < NUM_THROTTLES; i++) {
                int raw   = encoders.getDelta(i);
                int delta = constrain(raw, -2, 2);
                if (delta != 0) {
                    noteActivity();
                    if (activeScreen == Screen::ROSTER && i == 0) {
                        int newScroll = constrain(rosterScroll + delta, 0,
                                                  max(0, rosterCount - Display::ROSTER_VISIBLE_ROWS));
                        if (newScroll != rosterScroll) {
                            rosterScroll = newScroll;
                            if (xSemaphoreTake(rosterMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                                display.drawRosterScreen(rosterEntries, rosterCount, rosterScroll,
                                                         locoState[selectedThrottle].address, rosterReady);
                                xSemaphoreGive(rosterMutex);
                            }
                        }
                    } else {
                        uint32_t now     = millis();
                        uint32_t elapsed = now - lastEncMs[i];
                        lastEncMs[i]     = now;
                        int accel = (elapsed < 50) ? 8 : (elapsed < 100) ? 4 : (elapsed < 200) ? 2 : 1;
                        int newSpeed = constrain(locoState[i].speed + delta * accel, 0, 126);
                        if (newSpeed != locoState[i].speed) {
                            locoState[i].speed = newSpeed;
                            lastLocalInputMs[i] = millis();
                            if (activeScreen == Screen::THROTTLE)
                                display.drawThrottleSpeed(i, locoState[i]);
                            UICmd cmd{ UICmdType::SET_THROTTLE, (uint8_t)i, locoState[i] };
                            xQueueSend(cmdQueue, &cmd, 0);
                        }
                    }
                }
                if (encoders.wasClicked(i)) {
                    noteActivity();
                    if (activeScreen == Screen::THROTTLE) {
                        locoState[i].forward = !locoState[i].forward;
                        locoState[i].speed   = 0;
                        lastLocalInputMs[i]  = millis();
                        display.drawThrottleSpeed(i, locoState[i]);
                        UICmd cmd{ UICmdType::SET_THROTTLE, (uint8_t)i, locoState[i] };
                        xQueueSend(cmdQueue, &cmd, 0);
                    }
                }
            }
        }

        // --- Touch input (ST7796 only) ---
        // Tap throttle screen → toggle direction for tapped column
        // Tap roster screen  → scroll up (upper half) or down (lower half)
        // Touch input (ST7796 only)
        // - Top bar (ty < TOP_BAR_H): power / stop buttons
        // - Sub-header (TOP_BAR_H ≤ ty < HDR_H): tap throttle column → open roster for that slot;
        //   tap while on roster → go back to throttle
        // - Body on throttle screen: tap column → toggle direction
        // - Body on roster screen: tap top/bottom half → scroll up/down
        // - Any tap while sleeping → wake only, no action
#if DISPLAY_480
        {
            static uint32_t lastTouchMs = 0;
            uint16_t tx, ty;
            if ((millis() - lastTouchMs) >= 280 && display.getTouch(tx, ty)) {
                lastTouchMs = millis();
                bool wasSleeping = noteActivity();
                if (!wasSleeping) {
                    if (ty < Display::HDR_H) {
                        if (activeScreen == Screen::ROSTER) {
                            // Entire header → back to throttle
                            activeScreen = Screen::THROTTLE;
                            display.drawThrottleScreen(locoState, NUM_THROTTLES, connected,
                                                       trackPower, currentMa, locoName);
                        } else if (ty < Display::TOP_BAR_H) {
                            // Throttle screen: button row (PWR / STOP)
                            TopBarZone zone = display.hitTestTopBar(tx, ty);
                            if (zone == TopBarZone::POWER_BTN) {
                                UICmd cmd{ trackPower ? UICmdType::POWER_OFF : UICmdType::POWER_ON, 0, {} };
                                xQueueSend(cmdQueue, &cmd, 0);
                            } else if (zone == TopBarZone::STOP_BTN) {
                                for (int i = 0; i < NUM_THROTTLES; i++) locoState[i].speed = 0;
                                UICmd cmd{ UICmdType::EMERGENCY_STOP, 0, {} };
                                xQueueSend(cmdQueue, &cmd, 0);
                                for (int i = 0; i < NUM_THROTTLES; i++)
                                    display.drawThrottleSpeed(i, locoState[i]);
                            }
                        } else {
                            // Throttle screen: sub-header tap → open roster for that column
                            selectedThrottle = constrain((int)tx / (SCREEN_W / NUM_THROTTLES),
                                                         0, NUM_THROTTLES - 1);
                            activeScreen = Screen::ROSTER;
                            rosterScroll = 0;
                            if (!rosterReady) {
                                UICmd cmd{ UICmdType::REQUEST_ROSTER, 0, {} };
                                xQueueSend(cmdQueue, &cmd, 0);
                            }
                            if (xSemaphoreTake(rosterMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                                display.drawRosterScreen(rosterEntries, rosterCount, rosterScroll,
                                                         locoState[selectedThrottle].address, rosterReady);
                                xSemaphoreGive(rosterMutex);
                            }
                        }
                    } else if (activeScreen == Screen::THROTTLE) {
                        // Body: toggle direction for tapped column
                        int col = tx / (SCREEN_W / NUM_THROTTLES);
                        if (col >= 0 && col < NUM_THROTTLES) {
                            locoState[col].forward = !locoState[col].forward;
                            locoState[col].speed   = 0;
                            display.drawThrottleSpeed(col, locoState[col]);
                            UICmd cmd{ UICmdType::SET_THROTTLE, (uint8_t)col, locoState[col] };
                            xQueueSend(cmdQueue, &cmd, 0);
                        }
                    } else {
                        // Roster body: tap a row to assign that loco to the active throttle slot
                        int rowIdx = ((int)ty - Display::HDR_H) / Display::ROSTER_ROW_H;
                        int entryIdx = rosterScroll + rowIdx;
                        if (rowIdx >= 0 && rowIdx < Display::ROSTER_VISIBLE_ROWS
                                && entryIdx < rosterCount) {
                            if (xSemaphoreTake(rosterMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                                int newAddr = rosterEntries[entryIdx].address;
                                strncpy(locoName[selectedThrottle], rosterEntries[entryIdx].name,
                                        sizeof(locoName[0]) - 1);
                                xSemaphoreGive(rosterMutex);

#if DISPLAY_480
                                // Persist selection so it survives reset
                                {
                                    Preferences prefs;
                                    prefs.begin("locos", false);
                                    char addrKey[6], nameKey[6];
                                    snprintf(addrKey, sizeof(addrKey), "addr%d", selectedThrottle);
                                    snprintf(nameKey, sizeof(nameKey), "name%d", selectedThrottle);
                                    prefs.putInt(addrKey, newAddr);
                                    prefs.putString(nameKey, locoName[selectedThrottle]);
                                    prefs.end();
                                }
#endif

                                // Stop old loco before reassigning
                                UICmd stopOld{ UICmdType::SET_THROTTLE,
                                               (uint8_t)selectedThrottle, locoState[selectedThrottle] };
                                stopOld.loco.speed = 0;
                                xQueueSend(cmdQueue, &stopOld, 0);

                                locoState[selectedThrottle].address = newAddr;
                                locoState[selectedThrottle].speed   = 0;
                                locoState[selectedThrottle].forward = true;

                                UICmd assignCmd{ UICmdType::ASSIGN_LOCO,
                                                 (uint8_t)selectedThrottle, locoState[selectedThrottle] };
                                xQueueSend(cmdQueue, &assignCmd, 0);
                            }
                            activeScreen = Screen::THROTTLE;
                            // Roster screen left background black — skip fill to avoid flash
                            display.drawThrottleScreen(locoState, NUM_THROTTLES, connected,
                                                       trackPower, currentMa, locoName);
                        }
                    }
                }
            }
        }
#endif

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
                            display.drawRosterScreen(rosterEntries, rosterCount, rosterScroll,
                                         locoState[selectedThrottle].address, rosterReady);
                            xSemaphoreGive(rosterMutex);
                        }
                        if (!rosterReady) {
                            UICmd cmd{ UICmdType::REQUEST_ROSTER, 0, {} };
                            xQueueSend(cmdQueue, &cmd, 0);
                        }
                    } else {
                        activeScreen = Screen::THROTTLE;
                        display.drawThrottleScreen(locoState, NUM_THROTTLES, connected,
                                                   trackPower, currentMa, locoName);
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
                    display.drawThrottleScreen(locoState, NUM_THROTTLES, connected,
                                               trackPower, currentMa, locoName);
                    display.fadeInBacklight();
                } else if (activeScreen == Screen::THROTTLE) {
                    for (int i = 0; i < NUM_THROTTLES; i++)
                        display.drawThrottleSpeed(i, locoState[i]);
                }
            }
            estopBtnPrev = estopBtnNow;
        }

        // --- Inactivity sleep (suppressed while track power is on) ---
#if SLEEP_TIMEOUT_MS > 0
        if (!displaySleeping && !trackPower && (millis() - lastActivityMs) >= SLEEP_TIMEOUT_MS) {
            displaySleeping = true;
            display.sleep();
        }
#endif

        // --- Loop timing + periodic diagnostics ---
        uint32_t loopMs = millis() - loopStart;
        if (loopMs > loopMaxMs) loopMaxMs = loopMs;

        if (millis() - lastDiagMs >= 10000) {
            lastDiagMs = millis();
            Serial.printf("[UI]  stack free: %4u words  heap: %6u B  loop max: %ums\n",
                          uxTaskGetStackHighWaterMark(NULL),
                          esp_get_free_heap_size(),
                          loopMaxMs);
            loopMaxMs = 0;
        }

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
