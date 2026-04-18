#include "TestHarness.h"
#include "../config.h"
#include "../SharedTypes.h"

static const int ADDRESSES[NUM_THROTTLES] = {
    LOCO_ADDR_0, LOCO_ADDR_1, LOCO_ADDR_2, LOCO_ADDR_3
};

static void testTask(void *param) {
    auto eventQueue = static_cast<QueueHandle_t>(param);

    // Announce connected so the UI shows green dots
    DCCEvent connEvt{ DCCEventType::CONNECTED, {} };
    xQueueSend(eventQueue, &connEvt, portMAX_DELAY);

    bool forward = true;
    for (;;) {
        for (int slot = 0; slot < NUM_THROTTLES; slot++) {
            for (int spd = 0; spd <= 126; spd++) {
                DCCEvent evt{};
                evt.type         = DCCEventType::LOCO_UPDATE;
                evt.loco.address = ADDRESSES[slot];
                evt.loco.speed   = spd;
                evt.loco.forward = forward;
                xQueueSend(eventQueue, &evt, 0);
                vTaskDelay(pdMS_TO_TICKS(TEST_SPEED_STEP_MS));
            }
            DCCEvent resetEvt{};
            resetEvt.type         = DCCEventType::LOCO_UPDATE;
            resetEvt.loco.address = ADDRESSES[slot];
            resetEvt.loco.speed   = 0;
            resetEvt.loco.forward = forward;
            xQueueSend(eventQueue, &resetEvt, 0);
            vTaskDelay(pdMS_TO_TICKS(400));
        }
        forward = !forward;   // alternate direction each full cycle
    }
}

void testHarnessStart(QueueHandle_t eventQueue) {
    xTaskCreatePinnedToCore(
        testTask, "TEST",
        4096, eventQueue,
        1, nullptr,
        DCC_TASK_CORE
    );
}
