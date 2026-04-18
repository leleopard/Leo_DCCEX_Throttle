#include "UITask.h"
#include "../config.h"
#include "../SharedTypes.h"
#include "../ui/Display.h"
#include "../input/EncoderManager.h"

extern RosterEntry       rosterEntries[MAX_ROSTER_SIZE];
extern int               rosterCount;
extern SemaphoreHandle_t rosterMutex;

enum class Screen { THROTTLE, ROSTER };

static void uiTask(void *param) {
    auto *queues = reinterpret_cast<QueueHandle_t *>(param);
    QueueHandle_t eventQueue = queues[0];
    QueueHandle_t cmdQueue   = queues[1];

    static Display display;
    display.begin();

    static EncoderManager encoders;
    encoders.begin();

    LocoState locoState[NUM_THROTTLES];
    locoState[0].address = LOCO_ADDR_0;
    locoState[1].address = LOCO_ADDR_1;
    locoState[2].address = LOCO_ADDR_2;
    locoState[3].address = LOCO_ADDR_3;

    Screen activeScreen = Screen::THROTTLE;
    bool   connected    = false;
    bool   rosterReady  = false;
    int    rosterScroll = 0;

    bool rosterBtnPrev = HIGH;
    bool estopBtnPrev  = HIGH;

    display.drawThrottleScreen(locoState, NUM_THROTTLES, connected);

    DCCEvent evt;
    for (;;) {
        // --- Process incoming DCC events ---
        while (xQueueReceive(eventQueue, &evt, 0) == pdTRUE) {
            switch (evt.type) {
                case DCCEventType::CONNECTED:
                    connected = true;
                    if (activeScreen == Screen::THROTTLE)
                        display.drawThrottleScreen(locoState, NUM_THROTTLES, connected);
                    break;

                case DCCEventType::DISCONNECTED:
                    connected = false;
                    if (activeScreen == Screen::THROTTLE)
                        display.drawThrottleScreen(locoState, NUM_THROTTLES, connected);
                    break;

                case DCCEventType::ROSTER_READY:
                    rosterReady = true;
                    if (activeScreen == Screen::ROSTER) {
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
                            if (activeScreen == Screen::THROTTLE) {
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
            for (int i = 0; i < NUM_THROTTLES; i++) {
                int delta = encoders.getDelta(i);
                if (delta != 0) {
                    int newSpeed = constrain(locoState[i].speed + delta * SPEED_STEP, 0, 126);
                    if (newSpeed != locoState[i].speed) {
                        locoState[i].speed = newSpeed;
                        display.drawThrottleSpeed(i, locoState[i]);
                        UICmd cmd{ UICmdType::SET_THROTTLE, (uint8_t)i, locoState[i] };
                        xQueueSend(cmdQueue, &cmd, 0);
                    }
                }
                if (encoders.wasClicked(i)) {
                    locoState[i].forward = !locoState[i].forward;
                    locoState[i].speed   = 0;
                    display.drawThrottleColumn(i, locoState[i], connected);
                    UICmd cmd{ UICmdType::SET_THROTTLE, (uint8_t)i, locoState[i] };
                    xQueueSend(cmdQueue, &cmd, 0);
                }
            }
        }

        // --- Encoder 0: roster scroll ---
        if (activeScreen == Screen::ROSTER) {
            int delta = encoders.getDelta(0);
            if (delta != 0) {
                rosterScroll = constrain(rosterScroll + delta, 0,
                                         max(0, rosterCount - Display::ROSTER_VISIBLE_ROWS));
                if (xSemaphoreTake(rosterMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    display.drawRosterScreen(rosterEntries, rosterCount, rosterScroll);
                    xSemaphoreGive(rosterMutex);
                }
            }
        }

        // --- Roster button ---
        bool rosterBtnNow = digitalRead(BTN_ROSTER);
        if (rosterBtnPrev == HIGH && rosterBtnNow == LOW) {
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
        rosterBtnPrev = rosterBtnNow;

        // --- E-stop button ---
        bool estopBtnNow = digitalRead(BTN_ESTOP);
        if (estopBtnPrev == HIGH && estopBtnNow == LOW) {
            for (int i = 0; i < NUM_THROTTLES; i++)
                locoState[i].speed = 0;
            UICmd cmd{ UICmdType::EMERGENCY_STOP, 0, {} };
            xQueueSend(cmdQueue, &cmd, 0);
            if (activeScreen == Screen::THROTTLE)
                display.drawThrottleScreen(locoState, NUM_THROTTLES, connected);
        }
        estopBtnPrev = estopBtnNow;

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
