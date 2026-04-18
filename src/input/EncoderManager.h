#pragma once
#include <ESP32Encoder.h>

static constexpr int NUM_ENCODERS = 4;

// Manages all rotary encoders and their click buttons.
// getDelta() returns the count change since last call (thread-safe on Core 1 only).
// wasClicked() returns true once per button press (debounced).
class EncoderManager {
public:
    void begin();
    int  getDelta(int index);    // signed detent count since last call
    bool wasClicked(int index);  // true if button pressed since last call

private:
    ESP32Encoder _encoders[NUM_ENCODERS];
    int64_t      _lastCount[NUM_ENCODERS] = {};

    // Button debounce state
    bool     _btnPrev[NUM_ENCODERS]    = {};
    uint32_t _btnLastMs[NUM_ENCODERS]  = {};
    bool     _clicked[NUM_ENCODERS]    = {};

    static constexpr uint32_t DEBOUNCE_MS = 50;

    void pollButtons();

    static const uint8_t CLK_PINS[NUM_ENCODERS];
    static const uint8_t DT_PINS[NUM_ENCODERS];
    static const uint8_t SW_PINS[NUM_ENCODERS];
};
