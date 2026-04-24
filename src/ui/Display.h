#pragma once
#include <TFT_eSPI.h>
#include "../SharedTypes.h"
#include "../config.h"

#if DISPLAY_480
enum class TopBarZone { NONE, POWER_BTN, STOP_BTN };
#endif

class Display {
public:
#if DISPLAY_480
    static constexpr int ROSTER_VISIBLE_ROWS = 6;
    static constexpr int HDR_H               = 76;  // top bar (40) + col sub-header (36)
    static constexpr int TOP_BAR_H           = 40;  // button/mA row only
    static constexpr int ROSTER_ROW_H        = 36;
#else
    static constexpr int ROSTER_VISIBLE_ROWS = 5;
    static constexpr int HDR_H               = 25;
    static constexpr int ROSTER_ROW_H        = 32;
#endif

    void begin();
    void sleep();
    void wake();
    void fadeInBacklight();

    void drawThrottleScreen(const LocoState *locos, int count, bool connected,
                            bool trackPower = false, int milliAmps = -1,
                            const char (*names)[32] = nullptr);
    void drawTopBar(bool trackPower);
    void drawCurrentReading(int milliAmps);
    void drawColHeaders(bool connected, const LocoState *locos, int count,
                        const char (*names)[32] = nullptr);
    void drawThrottleColumn(int col, const LocoState &loco, bool connected);
    void drawThrottleSpeed(int col, const LocoState &loco);
    void drawRosterScreen(const RosterEntry *entries, int count, int scrollOffset,
                          int highlightAddr = -1, bool received = false);

#if DISPLAY_480
    bool        getTouch(uint16_t &x, uint16_t &y);
    void        runCalibration(uint16_t* calData);
    void        applyCalibration(uint16_t* calData);
    TopBarZone  hitTestTopBar(uint16_t tx, uint16_t ty);
#endif

private:
    TFT_eSPI    _tft;
    TFT_eSprite _face{&_tft};   // gauge face sprite: arc + ticks + needle + hub

    void renderFaceToSprite(int col, const LocoState &loco);
    void pushFaceSprite(int col);
};
