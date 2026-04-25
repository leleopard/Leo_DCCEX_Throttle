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
    // Function screen: Fn nav strip sits below gauge (GAUGE_CY=177 + SPRITE_R=101)
    static constexpr int FN_STRIP_Y          = 278; // first y of Fn nav strip on throttle screen
    static constexpr int FN_COLS             = 4;
    static constexpr int FN_ROWS             = 5;
    static constexpr int FN_BTN_W            = 120; // SCREEN_W / FN_COLS
    static constexpr int FN_BTN_H            = 48;  // (320 - HDR_H) / FN_ROWS = 244/5
    // Mini function strip on throttle screen: 3 fn buttons + 1 '+' button per column
    static constexpr int MINI_BTN_COUNT      = 3;   // function buttons per throttle column
    static constexpr int MINI_BTN_W          = 60;  // COL_W / (MINI_BTN_COUNT + 1) = 240/4
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
    void drawFunctionScreen(int slot, const LocoState &loco, const char *locoName,
                            const LocoFunctionData &funcData, int scroll = 0);
    // Redraws a single function button without touching the rest of the screen.
    // No-op if fnNum is outside the visible scroll window.
    void drawFnButton(int fnNum, const LocoFunctionData &funcData, int scroll);
    // Mini function strip on the throttle screen (below the gauge).
    // drawMiniFunctions redraws all 4 slots (3 fn buttons + '+') for one column.
    // drawMiniFnButton redraws a single slot (btnIdx 0-2 = fn, 3 = '+').
    void drawMiniFunctions(int col, const LocoFunctionData &funcData);
    void drawMiniFnButton(int col, int btnIdx, const LocoFunctionData &funcData);
#endif

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
#if DISPLAY_480
    void drawFnIcon(const char *name, int fnNum, uint16_t color, uint16_t bg, int cx, int cy);
#endif
};
