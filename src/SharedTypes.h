#pragma once
#include <Arduino.h>
#include "config.h"

// ---------------------------------------------------------------------------
// Loco state — shared representation used in queue messages
// ---------------------------------------------------------------------------
struct LocoState {
    int  address = DEFAULT_LOCO_ADDRESS;
    int  speed   = 0;    // 0-126
    bool forward = true;
};

// ---------------------------------------------------------------------------
// Roster entry — populated by DCC task, read by UI task
// Access is guarded by rosterMutex (defined in main.cpp)
// ---------------------------------------------------------------------------
struct RosterEntry {
    int  address = 0;
    char name[32] = {};
};

// ---------------------------------------------------------------------------
// DCC task → UI task  (events about command-station state)
// ---------------------------------------------------------------------------
enum class DCCEventType : uint8_t {
    CONNECTED,
    DISCONNECTED,
    ROSTER_READY,    // rosterEntries[] / rosterCount updated
    LOCO_UPDATE,     // speed or direction changed
    TRACK_POWER_ON,
    TRACK_POWER_OFF,
    CURRENT_UPDATE,  // track current reading in mA
};

struct DCCEvent {
    DCCEventType type;
    LocoState    loco;   // valid for LOCO_UPDATE
    int          value;  // valid for CURRENT_UPDATE (mA)
};

// ---------------------------------------------------------------------------
// UI task → DCC task  (user commands)
// ---------------------------------------------------------------------------
enum class UICmdType : uint8_t {
    SET_THROTTLE,
    EMERGENCY_STOP,
    REQUEST_ROSTER,
    POWER_ON,
    POWER_OFF,
};

struct UICmd {
    UICmdType type;
    uint8_t   index;  // throttle slot (0-3) for SET_THROTTLE
    LocoState loco;   // valid for SET_THROTTLE
};
