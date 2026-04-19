#pragma once
#include <TFT_eSPI.h>
#include "../SharedTypes.h"
#include "../config.h"

class Display {
public:
#if DISPLAY_480
    static constexpr int ROSTER_VISIBLE_ROWS = 6;
#else
    static constexpr int ROSTER_VISIBLE_ROWS = 5;
#endif

    void begin();
    void drawThrottleScreen(const LocoState *locos, int count, bool connected);
    void drawThrottleColumn(int col, const LocoState &loco, bool connected);
    void drawThrottleSpeed(int col, const LocoState &loco);   // bar + speed only
    void drawRosterScreen(const RosterEntry *entries, int count, int scrollOffset);

#if DISPLAY_480
    bool getTouch(uint16_t &x, uint16_t &y);
#endif

private:
    TFT_eSPI _tft;

    void drawHeader(const char *title, uint16_t bg, uint16_t fg);
    void drawStatusBar(bool connected);
};
