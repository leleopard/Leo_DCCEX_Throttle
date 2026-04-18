#include "DCCDelegate.h"
#include "../config.h"

// Shared roster storage — written here, read by UI task
extern RosterEntry rosterEntries[MAX_ROSTER_SIZE];
extern int         rosterCount;
extern SemaphoreHandle_t rosterMutex;

// Reference to the global protocol object (owned by DCCTask)
extern DCCEXProtocol dccexProtocol;

void DCCDelegate::postEvent(const DCCEvent &evt) {
    xQueueSendFromISR(_eventQueue, &evt, nullptr);
}

void DCCDelegate::receivedServerVersion(int major, int minor, int patch) {
    Serial.printf("[DCC] Connected — EX-CommandStation v%d.%d.%d\n", major, minor, patch);
    DCCEvent evt{ DCCEventType::CONNECTED, {} };
    postEvent(evt);
}

void DCCDelegate::receivedRosterList() {
    if (xSemaphoreTake(rosterMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        rosterCount = 0;
        for (Loco *loco = dccexProtocol.roster->getFirst();
             loco && rosterCount < MAX_ROSTER_SIZE;
             loco = loco->getNext()) {
            rosterEntries[rosterCount].address = loco->getAddress();
            strncpy(rosterEntries[rosterCount].name,
                    loco->getName() ? loco->getName() : "",
                    sizeof(rosterEntries[0].name) - 1);
            rosterCount++;
        }
        xSemaphoreGive(rosterMutex);
    }
    DCCEvent evt{ DCCEventType::ROSTER_READY, {} };
    postEvent(evt);
}

void DCCDelegate::receivedLocoUpdate(Loco *loco) {
    if (!loco) return;
    DCCEvent evt{};
    evt.type         = DCCEventType::LOCO_UPDATE;
    evt.loco.address = loco->getAddress();
    evt.loco.speed   = loco->getSpeed();
    evt.loco.forward = (loco->getDirection() == Direction::Forward);
    postEvent(evt);
}

void DCCDelegate::receivedLocoBroadcast(int address, int speed,
                                        Direction direction, int /*functionMap*/) {
    DCCEvent evt{};
    evt.type         = DCCEventType::LOCO_UPDATE;
    evt.loco.address = address;
    evt.loco.speed   = speed;
    evt.loco.forward = (direction == Direction::Forward);
    postEvent(evt);
}
