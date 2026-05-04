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
// Function data — populated by DCC task when roster arrives or loco changes
// Access is guarded by functionMutex (defined in DCCTask.cpp)
// ---------------------------------------------------------------------------
static const int MAX_LOCO_FUNCTIONS = 32;
static const int FUNC_NAME_LEN      = 16;

struct FunctionDef {
    char name[FUNC_NAME_LEN];
    bool momentary;
};

struct LocoFunctionData {
    FunctionDef defs[MAX_LOCO_FUNCTIONS];
    int32_t     states;
    bool        valid;
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
    FUNCTION_UPDATE, // function states changed; loco.address = DCC addr, value = int32 bitmask
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
    ASSIGN_LOCO,   // reassign throttle slot to a different address
    FUNCTION_CMD,  // send function on/off; index=slot, func=fn#, loco.forward=on/off
};

struct UICmd {
    UICmdType type;
    uint8_t   index;  // throttle slot (0-3)
    uint8_t   func;   // function number for FUNCTION_CMD
    LocoState loco;   // valid for SET_THROTTLE / ASSIGN_LOCO; loco.forward=on for FUNCTION_CMD
};
