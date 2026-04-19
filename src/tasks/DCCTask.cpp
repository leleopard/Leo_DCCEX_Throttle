#include "DCCTask.h"
#include <DCCEXProtocol.h>
#include "../config.h"
#include "../SharedTypes.h"
#include "../dcc/DCCDelegate.h"

DCCEXProtocol dccexProtocol;

RosterEntry      rosterEntries[MAX_ROSTER_SIZE];
int              rosterCount = 0;
SemaphoreHandle_t rosterMutex;

static const int LOCO_ADDRESSES[NUM_THROTTLES] = {
    LOCO_ADDR_0, LOCO_ADDR_1
};

static Loco *activeLoco[NUM_THROTTLES] = {};

static void applyThrottle(int index, const LocoState &s) {
    if (!activeLoco[index]) return;
    Direction dir = s.forward ? Direction::Forward : Direction::Reverse;
    dccexProtocol.setThrottle(activeLoco[index], s.speed, dir);
}

static void dccTask(void *param) {
    auto *queues = reinterpret_cast<QueueHandle_t *>(param);
    QueueHandle_t eventQueue = queues[0];
    QueueHandle_t cmdQueue   = queues[1];

    rosterMutex = xSemaphoreCreateMutex();

    DCCEX_SERIAL.begin(DCCEX_BAUD, SERIAL_8N1, DCCEX_RX_PIN, DCCEX_TX_PIN);
    delay(500);

    DCCDelegate delegate(eventQueue);
    dccexProtocol.setLogStream(&Serial);
    dccexProtocol.setDelegate(&delegate);
    dccexProtocol.connect(&DCCEX_SERIAL);

    dccexProtocol.getLists(true, false, false, false);

    UICmd cmd;
    for (;;) {
        dccexProtocol.check();

        // Resolve active locos after roster arrives
        if (dccexProtocol.receivedRoster()) {
            for (int i = 0; i < NUM_THROTTLES; i++) {
                if (!activeLoco[i]) {
                    activeLoco[i] = dccexProtocol.findLocoInRoster(LOCO_ADDRESSES[i]);
                    if (!activeLoco[i])
                        activeLoco[i] = new Loco(LOCO_ADDRESSES[i], LocoSource::LocoSourceEntry);
                }
            }
        }

        while (xQueueReceive(cmdQueue, &cmd, 0) == pdTRUE) {
            switch (cmd.type) {
                case UICmdType::SET_THROTTLE:
                    applyThrottle(cmd.index, cmd.loco);
                    break;
                case UICmdType::EMERGENCY_STOP:
                    dccexProtocol.emergencyStop();
                    break;
                case UICmdType::REQUEST_ROSTER:
                    dccexProtocol.getLists(true, false, false, false);
                    break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void dccTaskStart(QueueHandle_t eventQueue, QueueHandle_t cmdQueue) {
    static QueueHandle_t queues[2];
    queues[0] = eventQueue;
    queues[1] = cmdQueue;

    xTaskCreatePinnedToCore(
        dccTask, "DCC",
        DCC_TASK_STACK, queues,
        DCC_TASK_PRIORITY, nullptr,
        DCC_TASK_CORE
    );
}
