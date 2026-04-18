#include <Arduino.h>
#include "config.h"
#include "SharedTypes.h"
#include "tasks/DCCTask.h"
#include "tasks/UITask.h"
#if ENABLE_TEST_HARNESS
#include "test/TestHarness.h"
#endif

// Inter-task queues (created before tasks start)
static QueueHandle_t dccEventQueue;  // DCC task → UI task
static QueueHandle_t uiCmdQueue;     // UI task  → DCC task

void setup() {
    Serial.begin(115200);
    Serial.println("[Leo] Booting...");

    pinMode(BTN_ROSTER, INPUT_PULLUP);
    pinMode(BTN_ESTOP,  INPUT_PULLUP);

    dccEventQueue = xQueueCreate(DCC_EVENT_QUEUE_DEPTH, sizeof(DCCEvent));
    uiCmdQueue    = xQueueCreate(UI_CMD_QUEUE_DEPTH,    sizeof(UICmd));

    configASSERT(dccEventQueue);
    configASSERT(uiCmdQueue);

#if ENABLE_TEST_HARNESS
    testHarnessStart(dccEventQueue);
#else
    dccTaskStart(dccEventQueue, uiCmdQueue);
#endif
    uiTaskStart(dccEventQueue, uiCmdQueue);

    Serial.println("[Leo] Tasks started.");
}

void loop() {
    // All work is in FreeRTOS tasks; loop() is intentionally idle.
    vTaskDelay(portMAX_DELAY);
}
