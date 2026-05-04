#include "DCCTask.h"
#include <DCCEXProtocol.h>
#include "../config.h"
#include "../SharedTypes.h"
#include "../dcc/DCCDelegate.h"

DCCEXProtocol dccexProtocol;

RosterEntry      rosterEntries[MAX_ROSTER_SIZE];
int              rosterCount = 0;
SemaphoreHandle_t rosterMutex;

LocoFunctionData  locoFuncData[NUM_THROTTLES];
SemaphoreHandle_t functionMutex;

static const int LOCO_ADDRESSES[NUM_THROTTLES] = {
    LOCO_ADDR_0, LOCO_ADDR_1
};

static Loco *activeLoco[NUM_THROTTLES] = {};

static void populateFunctionData(int slot) {
    LocoFunctionData &fd = locoFuncData[slot];
    fd.states = 0; fd.valid = false;
    if (!activeLoco[slot]) return;
    fd.states = activeLoco[slot]->getFunctionStates();
    fd.valid  = true;
    for (int f = 0; f < MAX_LOCO_FUNCTIONS; f++) {
        const char *n = activeLoco[slot]->getFunctionName(f);
        if (n && n[0]) {
            strncpy(fd.defs[f].name, n, FUNC_NAME_LEN - 1);
            fd.defs[f].name[FUNC_NAME_LEN - 1] = '\0';
            fd.defs[f].momentary = activeLoco[slot]->isFunctionMomentary(f);
        } else {
            fd.defs[f].name[0]   = '\0';
            fd.defs[f].momentary = false;
        }
    }
}

static void applyThrottle(int index, const LocoState &s) {
    if (!activeLoco[index]) return;
    Direction dir = s.forward ? Direction::Forward : Direction::Reverse;
    dccexProtocol.setThrottle(activeLoco[index], s.speed, dir);
}

static void dccTask(void *param) {
    auto *queues = reinterpret_cast<QueueHandle_t *>(param);
    QueueHandle_t eventQueue = queues[0];
    QueueHandle_t cmdQueue   = queues[1];

    rosterMutex   = xSemaphoreCreateMutex();
    functionMutex = xSemaphoreCreateMutex();

    DCCEX_SERIAL.begin(DCCEX_BAUD, SERIAL_8N1, DCCEX_RX_PIN, DCCEX_TX_PIN);
    delay(500);
    // Send raw commands before the protocol library connects — guaranteed delivery
    // even if the library handshake hasn't completed yet.  Covers reset/reboot.
    // (Hardware power loss cannot be intercepted in software.)
    DCCEX_SERIAL.println("<!>");   // emergency stop all locos
    DCCEX_SERIAL.println("<0>");   // power off all tracks
    delay(100);
    Serial.println("[DCC] Serial2 started, emergency stop + power off sent, connecting...");

    DCCDelegate delegate(eventQueue);
    dccexProtocol.setLogStream(&Serial);
    dccexProtocol.setDebug(false);
    dccexProtocol.setDelegate(&delegate);
    dccexProtocol.connect(&DCCEX_SERIAL);
    // Pre-create loco objects so throttles work immediately without waiting for
    // the roster response (which can take several seconds to arrive).
    for (int i = 0; i < NUM_THROTTLES; i++)
        activeLoco[i] = new Loco(LOCO_ADDRESSES[i], LocoSource::LocoSourceEntry);
    dccexProtocol.requestServerVersion();
    dccexProtocol.getLists(true, false, false, false);
    Serial.println("[DCC] connect() and getLists() sent");

    UICmd cmd;
    uint32_t lastCurrentPollMs = 0;
    uint32_t lastStatusPollMs  = 0;
    uint32_t lastRosterPollMs  = 0;
    uint32_t lastDiagMs        = 0;
    for (;;) {
        dccexProtocol.check();

        uint32_t now = millis();

        // Poll track current every 2 seconds
        if (now - lastCurrentPollMs >= 1000) {
            lastCurrentPollMs = now;
            dccexProtocol.requestTrackCurrents();
        }

        // Poll full status (power state) every 10 seconds to stay in sync
        // with JMRI or any external source that may change track power
        if (now - lastStatusPollMs >= 10000) {
            lastStatusPollMs = now;
            dccexProtocol.requestServerVersion();
        }

        // Retry roster request every 8 seconds until received
        if (!dccexProtocol.receivedRoster() && now - lastRosterPollMs >= 8000) {
            lastRosterPollMs = now;
            Serial.println("[DCC] Roster not yet received — retrying <JR>");
            dccexProtocol.refreshRoster();
            dccexProtocol.getLists(true, false, false, false);
        }

        // The library sets receivedRoster()=true for empty rosters without calling the
        // delegate callback, so fire ROSTER_READY ourselves once the flag is set.
        static bool rosterReadyFired = false;
        if (!rosterReadyFired && dccexProtocol.receivedRoster()) {
            rosterReadyFired = true;
            Serial.printf("[DCC] Roster flag set by library (count via delegate: %d)\n", rosterCount);
            // Replace pre-created LocoSourceEntry objects with roster-derived Loco* so
            // that function names and momentary flags are available.
            if (xSemaphoreTake(functionMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
                for (int i = 0; i < NUM_THROTTLES; i++) {
                    Loco *found = dccexProtocol.findLocoInRoster(LOCO_ADDRESSES[i]);
                    if (found) activeLoco[i] = found;
                    populateFunctionData(i);
                }
                xSemaphoreGive(functionMutex);
            }
            DCCEvent evt{ DCCEventType::ROSTER_READY, {}, 0 };
            xQueueSend(eventQueue, &evt, 0);
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
                    dccexProtocol.refreshRoster();
                    dccexProtocol.getLists(true, false, false, false);
                    break;
                case UICmdType::POWER_ON:
                    dccexProtocol.powerOn();
                    break;
                case UICmdType::POWER_OFF:
                    dccexProtocol.powerOff();
                    break;
                case UICmdType::ASSIGN_LOCO: {
                    Loco *found = dccexProtocol.findLocoInRoster(cmd.loco.address);
                    activeLoco[cmd.index] = found
                        ? found
                        : new Loco(cmd.loco.address, LocoSource::LocoSourceEntry);
                    Serial.printf("[DCC] Throttle %d → addr %d\n", cmd.index, cmd.loco.address);
                    if (xSemaphoreTake(functionMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                        populateFunctionData(cmd.index);
                        xSemaphoreGive(functionMutex);
                    }
                    DCCEvent fEvt{};
                    fEvt.type         = DCCEventType::FUNCTION_UPDATE;
                    fEvt.loco.address = cmd.loco.address;
                    fEvt.value        = (int)locoFuncData[cmd.index].states;
                    xQueueSend(eventQueue, &fEvt, 0);
                    break;
                }
                case UICmdType::FUNCTION_CMD:
                    if (activeLoco[cmd.index]) {
                        if (cmd.loco.forward)
                            dccexProtocol.functionOn(activeLoco[cmd.index], cmd.func);
                        else
                            dccexProtocol.functionOff(activeLoco[cmd.index], cmd.func);
                    }
                    break;
            }
        }

        if (now - lastDiagMs >= 10000) {
            lastDiagMs = now;
            Serial.printf("[DCC] stack free: %4u words\n",
                          uxTaskGetStackHighWaterMark(NULL));
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
