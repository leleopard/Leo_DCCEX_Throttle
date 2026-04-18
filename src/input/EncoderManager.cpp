#include "EncoderManager.h"
#include "../config.h"
#include <Arduino.h>

const uint8_t EncoderManager::CLK_PINS[NUM_ENCODERS] = { ENC1_CLK, ENC2_CLK, ENC3_CLK, ENC4_CLK };
const uint8_t EncoderManager::DT_PINS[NUM_ENCODERS]  = { ENC1_DT,  ENC2_DT,  ENC3_DT,  ENC4_DT  };
const uint8_t EncoderManager::SW_PINS[NUM_ENCODERS]  = { ENC1_SW,  ENC2_SW,  ENC3_SW,  ENC4_SW  };

void EncoderManager::begin() {
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    for (int i = 0; i < NUM_ENCODERS; i++) {
        _encoders[i].attachHalfQuad(DT_PINS[i], CLK_PINS[i]);
        _encoders[i].setCount(0);
        _lastCount[i] = 0;

        pinMode(SW_PINS[i], INPUT_PULLUP);
        _btnPrev[i]   = HIGH;
        _clicked[i]   = false;
        _btnLastMs[i] = 0;
    }
}

int EncoderManager::getDelta(int index) {
    pollButtons();
    int64_t now = _encoders[index].getCount();
    // Half-quad gives 2 counts per detent; divide by 2 for clean steps
    int64_t delta = (now - _lastCount[index]) / 2;
    if (delta != 0) _lastCount[index] += delta * 2;
    return static_cast<int>(delta);
}

bool EncoderManager::wasClicked(int index) {
    pollButtons();
    if (_clicked[index]) {
        _clicked[index] = false;
        return true;
    }
    return false;
}

void EncoderManager::pollButtons() {
    uint32_t now = millis();
    for (int i = 0; i < NUM_ENCODERS; i++) {
        bool cur = digitalRead(SW_PINS[i]);
        if (_btnPrev[i] == HIGH && cur == LOW &&
            (now - _btnLastMs[i]) > DEBOUNCE_MS) {
            _clicked[i]   = true;
            _btnLastMs[i] = now;
        }
        _btnPrev[i] = cur;
    }
}
