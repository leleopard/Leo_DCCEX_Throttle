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
    ROSTER_READY,   // rosterEntries[] / rosterCount updated
    LOCO_UPDATE,    // speed or direction changed
};

struct DCCEvent {
    DCCEventType type;
    LocoState    loco;   // valid for LOCO_UPDATE
};

// ---------------------------------------------------------------------------
// UI task → DCC task  (user commands)
// ---------------------------------------------------------------------------
enum class UICmdType : uint8_t {
    SET_THROTTLE,
    EMERGENCY_STOP,
    REQUEST_ROSTER,
};

struct UICmd {
    UICmdType type;
    uint8_t   index;  // throttle slot (0-3) for SET_THROTTLE
    LocoState loco;   // valid for SET_THROTTLE
};
