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
    void drawThrottleSpeed(int col, const LocoState &loco);
    void drawRosterScreen(const RosterEntry *entries, int count, int scrollOffset);

#if DISPLAY_480
    bool getTouch(uint16_t &x, uint16_t &y);
    void runCalibration(uint16_t* calData);
    void applyCalibration(uint16_t* calData);
#endif

private:
    TFT_eSPI    _tft;
    TFT_eSprite _face{&_tft};  // gauge face sprite: arc + ticks + needle + hub

    void drawHeader(const char *title, uint16_t bg, uint16_t fg);
    void drawStatusBar(bool connected);
    void drawBezelAndDial(int col);
    void renderFaceToSprite(int col, int speed);   // render gauge face into _face sprite
    void pushFaceSprite(int col);                  // push _face to correct screen position
    void drawGaugeTexts(int col, const LocoState &loco);
};
