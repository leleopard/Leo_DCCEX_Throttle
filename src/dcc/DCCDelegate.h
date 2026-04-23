#pragma once
#include <DCCEXProtocol.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "../SharedTypes.h"

// Receives callbacks from DCCEXProtocol and forwards them as DCCEvents
// to the UI task via the dccEventQueue.
class DCCDelegate : public DCCEXProtocolDelegate {
public:
    explicit DCCDelegate(QueueHandle_t eventQueue) : _eventQueue(eventQueue) {}

    void receivedServerVersion(int major, int minor, int patch) override;
    void receivedRosterList() override;
    void receivedLocoUpdate(Loco *loco) override;
    void receivedLocoBroadcast(int address, int speed, Direction direction, int functionMap) override;
    void receivedTrackPower(TrackPower state) override;
    void receivedTrackCurrent(char track, int current) override;

private:
    QueueHandle_t _eventQueue;

    void postEvent(const DCCEvent &evt);
};
