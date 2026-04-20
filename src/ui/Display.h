#pragma once
#include <TFT_eSPI.h>
#include "../SharedTypes.h"
#include "../config.h"

class Display {
public:
#if DISPLAY_480
    static constexpr int ROSTER_VISIBLE_ROWS = 6;
    static constexpr int HDR_H               = 32;
#else
    static constexpr int ROSTER_VISIBLE_ROWS = 5;
    static constexpr int HDR_H               = 25;
#endif

    void begin();
    void sleep();
    void wake();

    void drawThrottleScreen(const LocoState *locos, int count, bool connected);
    void drawThrottleColumn(int col, const LocoState &loco, bool connected);
    void drawThrottleSpeed(int col, const LocoState &loco);   // flicker-free speed update
    void drawRosterScreen(const RosterEntry *entries, int count, int scrollOffset);

#if DISPLAY_480
    bool getTouch(uint16_t &x, uint16_t &y);
    void runCalibration(uint16_t* calData);
    void applyCalibration(uint16_t* calData);
#endif

private:
    TFT_eSPI _tft;
    float    _gaugeAngle[NUM_THROTTLES];  // current needle angle per slot (degrees)

    void drawHeader(const char *title, uint16_t bg, uint16_t fg);
    void drawStatusBar(bool connected);

    // Gauge layers — called in sequence to build the speedometer face
    void drawBezelAndDial(int col);                              // chrome bezel + dark dial face
    void drawGaugeTicks(int col);                                // tick marks + speed labels
    void drawGaugeArc(int col, int speed);                       // arc rings (background + filled)
    void drawGaugeNeedle(int col, float angleDeg, uint16_t color); // single needle draw/erase
    void drawGaugeHub(int col);                                  // centre hub cap
    void drawGaugeTexts(int col, const LocoState &loco);        // speed value + FWD/REV label
};
