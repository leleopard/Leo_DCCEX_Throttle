#include "Display.h"
#include "../config.h"

#if DISPLAY_480
#  include "splash_480.h"
#else
#  include "splash.h"
#endif

// ---------------------------------------------------------------------------
// Colour palette
// ---------------------------------------------------------------------------
static constexpr uint16_t COL_BG       = TFT_BLACK;
static constexpr uint16_t COL_HEADER   = 0x1082;
static constexpr uint16_t COL_TEXT     = TFT_WHITE;
static constexpr uint16_t COL_ACCENT   = TFT_YELLOW;
static constexpr uint16_t COL_FWD      = TFT_GREEN;
static constexpr uint16_t COL_REV      = TFT_RED;
static constexpr uint16_t COL_GAUGE_BG = 0x2104;   // dark grey arc background
static constexpr uint16_t COL_DIVIDER  = 0x4208;
static constexpr uint16_t COL_SELECTED = 0x1E3F;

// Per-throttle gauge colours
static constexpr uint16_t COL_BAR[4] = {
    TFT_CYAN, TFT_GREEN, TFT_YELLOW, TFT_MAGENTA
};

// ---------------------------------------------------------------------------
// Screen & column dimensions
// ---------------------------------------------------------------------------
#if DISPLAY_480
static constexpr int W     = 480;
static constexpr int H     = 320;
#else
static constexpr int W     = 320;
static constexpr int H     = 240;
#endif
static constexpr int COL_W = W / NUM_THROTTLES;
// HDR_H defined in Display.h as public constexpr
static constexpr int HDR_H = Display::HDR_H;

// ---------------------------------------------------------------------------
// Gauge layout — speedometer arc from GAUGE_START to GAUGE_START+GAUGE_SWEEP
// Angles in TFT_eSPI convention: 0 = 12 o'clock, clockwise.
// Arc sweeps from ~8 o'clock (220°) clockwise via top to ~4 o'clock (140°).
// ---------------------------------------------------------------------------
static constexpr int   GAUGE_START = 220;   // start angle (degrees)
static constexpr int   GAUGE_SWEEP = 280;   // total sweep (degrees)

#if DISPLAY_480
static constexpr int   GAUGE_R      = 100;  // outer radius
static constexpr int   GAUGE_THICK  = 22;   // arc ring thickness
static constexpr int   GAUGE_CY     = 148;  // gauge centre y
static constexpr int   NEEDLE_R     = 74;   // needle length
static constexpr int   NEEDLE_W     = 4;    // needle width (pixels)
static constexpr int   HUB_R        = 10;   // centre hub radius
static constexpr int   GAUGE_SPD_Y  = 210;  // speed text centre y (below arc endpoints)
static constexpr int   GAUGE_DIR_Y  = 250;  // direction text centre y
static constexpr int   SPD_BOX_W    = 100;  // clearance box for speed text
static constexpr int   SPD_BOX_H    = 38;
static constexpr int   DIR_BOX_W    = 80;
static constexpr int   DIR_BOX_H    = 30;

static constexpr int ROSTER_HDR_H  = 48;
static constexpr int ROSTER_ROW_H  = 36;
static constexpr int STATUS_H      = 28;
#else
static constexpr int   GAUGE_R      = 68;
static constexpr int   GAUGE_THICK  = 16;
static constexpr int   GAUGE_CY     = 100;
static constexpr int   NEEDLE_R     = 50;
static constexpr int   NEEDLE_W     = 3;
static constexpr int   HUB_R        = 7;
static constexpr int   GAUGE_SPD_Y  = 148;
static constexpr int   GAUGE_DIR_Y  = 178;
static constexpr int   SPD_BOX_W    = 80;
static constexpr int   SPD_BOX_H    = 32;
static constexpr int   DIR_BOX_W    = 64;
static constexpr int   DIR_BOX_H    = 24;

static constexpr int ROSTER_HDR_H  = 36;
static constexpr int ROSTER_ROW_H  = 32;
static constexpr int STATUS_H      = 24;
#endif

// ---------------------------------------------------------------------------

void Display::begin() {
#if TFT_BL_PIN >= 0
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);
#endif
    _tft.init();
    _tft.setRotation(1);
    _tft.pushImage(0, 0, SCREEN_W, SCREEN_H, splash_pixels);

    for (int i = 0; i < NUM_THROTTLES; i++)
        _gaugeAngle[i] = GAUGE_START;

    delay(2000);
}

// ---------------------------------------------------------------------------
// Gauge helpers
// ---------------------------------------------------------------------------

// Draw (or erase) a single needle at angleDeg using the given colour.
void Display::drawGaugeNeedle(int col, float angleDeg, uint16_t color) {
    int cx = col * COL_W + COL_W / 2;
    int cy = GAUGE_CY;
    float rad = angleDeg * DEG_TO_RAD;
    int nx = cx + (int)(NEEDLE_R * sinf(rad));
    int ny = cy - (int)(NEEDLE_R * cosf(rad));
    _tft.drawWideLine(cx, cy, nx, ny, NEEDLE_W, color, color);
}

// Draw the arc, ticks, needle, and hub — assumes area is already clear or
// caller has erased the old needle first.
void Display::drawGauge(int col, const LocoState &loco) {
    int cx = col * COL_W + COL_W / 2;
    int cy = GAUGE_CY;

    float needleAngle = GAUGE_START + (float)loco.speed / 126.0f * GAUGE_SWEEP;
    _gaugeAngle[col] = needleAngle;

    int ir = GAUGE_R - GAUGE_THICK;

    // Background arc (full sweep, dark)
    _tft.drawSmoothArc(cx, cy, GAUGE_R, ir,
                        GAUGE_START, GAUGE_START + GAUGE_SWEEP,
                        COL_GAUGE_BG, COL_BG);

    // Filled arc (speed portion, coloured)
    if (loco.speed > 0) {
        _tft.drawSmoothArc(cx, cy, GAUGE_R, ir,
                            GAUGE_START, (uint32_t)needleAngle,
                            COL_BAR[col & 3], COL_BG);
    }

    // Major tick marks at 0 / 25 / 50 / 75 / 100 %
    for (int t = 0; t <= 4; t++) {
        float tickRad = (GAUGE_START + t * GAUGE_SWEEP / 4.0f) * DEG_TO_RAD;
        int x1 = cx + (int)((ir - 2)  * sinf(tickRad));
        int y1 = cy - (int)((ir - 2)  * cosf(tickRad));
        int x2 = cx + (int)((ir - 12) * sinf(tickRad));
        int y2 = cy - (int)((ir - 12) * cosf(tickRad));
        _tft.drawLine(x1, y1, x2, y2, COL_TEXT);
    }

    // Needle
    drawGaugeNeedle(col, needleAngle, COL_TEXT);

    // Hub: filled circle with dark centre for a ring effect
    _tft.fillCircle(cx, cy, HUB_R,     COL_TEXT);
    _tft.fillCircle(cx, cy, HUB_R - 3, COL_GAUGE_BG);
}

// Speed value and direction label in the gap below the gauge centre.
void Display::drawGaugeTexts(int col, const LocoState &loco) {
    int cx = col * COL_W + COL_W / 2;

    // Speed
    _tft.fillRect(cx - SPD_BOX_W / 2, GAUGE_SPD_Y - SPD_BOX_H / 2,
                  SPD_BOX_W, SPD_BOX_H, COL_BG);
    _tft.setFreeFont(&FreeSansBold18pt7b);
    _tft.setTextColor(COL_TEXT);
    _tft.setTextDatum(MC_DATUM);
    char spd[4];
    snprintf(spd, sizeof(spd), "%d", loco.speed);
    _tft.drawString(spd, cx, GAUGE_SPD_Y);

    // Direction
    _tft.fillRect(cx - DIR_BOX_W / 2, GAUGE_DIR_Y - DIR_BOX_H / 2,
                  DIR_BOX_W, DIR_BOX_H, COL_BG);
    _tft.setFreeFont(&FreeSansBold12pt7b);
    _tft.setTextColor(loco.forward ? COL_FWD : COL_REV);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString(loco.forward ? "FWD" : "REV", cx, GAUGE_DIR_Y);

    _tft.setTextFont(1);
    _tft.setTextDatum(TL_DATUM);
}

// ---------------------------------------------------------------------------
// Throttle screen — full redraw of all columns
// ---------------------------------------------------------------------------
void Display::drawThrottleScreen(const LocoState *locos, int count, bool connected) {
    _tft.fillScreen(COL_BG);
    for (int i = 1; i < NUM_THROTTLES; i++)
        _tft.drawFastVLine(i * COL_W, 0, H, COL_DIVIDER);
    for (int i = 0; i < count && i < NUM_THROTTLES; i++)
        drawThrottleColumn(i, locos[i], connected);
}

// ---------------------------------------------------------------------------
// Single column full redraw — header + gauge + texts
// ---------------------------------------------------------------------------
void Display::drawThrottleColumn(int col, const LocoState &loco, bool connected) {
    int x = col * COL_W;

    // Header
    _tft.fillRect(x, 0, COL_W - 1, HDR_H, COL_HEADER);
    _tft.fillCircle(x + COL_W - 11, HDR_H / 2, 4, connected ? TFT_GREEN : TFT_RED);
    _tft.setFreeFont(&FreeSans9pt7b);
    _tft.setTextColor(COL_TEXT);
    _tft.setTextDatum(MC_DATUM);
    char addr[8];
    snprintf(addr, sizeof(addr), "#%d", loco.address);
    _tft.drawString(addr, x + (COL_W - 1 - 18) / 2, HDR_H / 2);
    _tft.drawFastHLine(x, HDR_H, COL_W - 1, COL_DIVIDER);

    // Clear gauge area and redraw
    _tft.fillRect(x, HDR_H + 1, COL_W - 1, H - HDR_H - 1, COL_BG);
    drawGauge(col, loco);
    drawGaugeTexts(col, loco);
}

// ---------------------------------------------------------------------------
// Speed-only update — flicker-free:
//   erase old needle → redraw arcs + ticks → draw new needle → update text
// ---------------------------------------------------------------------------
void Display::drawThrottleSpeed(int col, const LocoState &loco) {
    // 1. Erase old needle (draw in background colour)
    drawGaugeNeedle(col, _gaugeAngle[col], COL_BG);

    // 2. Redraw arcs, ticks, new needle, hub
    drawGauge(col, loco);

    // 3. Speed text only (direction unchanged)
    int cx = col * COL_W + COL_W / 2;
    _tft.fillRect(cx - SPD_BOX_W / 2, GAUGE_SPD_Y - SPD_BOX_H / 2,
                  SPD_BOX_W, SPD_BOX_H, COL_BG);
    _tft.setFreeFont(&FreeSansBold18pt7b);
    _tft.setTextColor(COL_TEXT);
    _tft.setTextDatum(MC_DATUM);
    char spd[4];
    snprintf(spd, sizeof(spd), "%d", loco.speed);
    _tft.drawString(spd, cx, GAUGE_SPD_Y);
    _tft.setTextFont(1);
    _tft.setTextDatum(TL_DATUM);
}

// ---------------------------------------------------------------------------
// Roster screen
// ---------------------------------------------------------------------------
void Display::drawRosterScreen(const RosterEntry *entries, int count, int scrollOffset) {
    _tft.fillScreen(COL_BG);
    drawHeader("Roster", COL_SELECTED, COL_TEXT);

    if (count == 0) {
        _tft.setTextSize(1);
        _tft.setTextColor(COL_TEXT, COL_BG);
        _tft.setCursor(16, 80);
        _tft.print("No roster received yet.");
        return;
    }

    int y       = ROSTER_HDR_H + 4;
    int visible = min(ROSTER_VISIBLE_ROWS, count - scrollOffset);

    for (int i = 0; i < visible; i++) {
        int idx        = scrollOffset + i;
        bool hilight   = (entries[idx].address == DEFAULT_LOCO_ADDRESS);
        uint16_t rowBg = hilight ? COL_SELECTED : COL_BG;

        _tft.fillRect(0, y, W, ROSTER_ROW_H - 2, rowBg);

        _tft.setTextSize(1);
        _tft.setTextColor(COL_ACCENT, rowBg);
        _tft.setCursor(6, y + 8);
        char addr[8];
        snprintf(addr, sizeof(addr), "#%-4d", entries[idx].address);
        _tft.print(addr);

        _tft.setTextColor(COL_TEXT, rowBg);
        _tft.setCursor(60, y + 8);
        _tft.print(entries[idx].name[0] ? entries[idx].name : "(unnamed)");

        y += ROSTER_ROW_H;
    }

    if (count > ROSTER_VISIBLE_ROWS) {
        _tft.setTextSize(1);
        _tft.setTextColor(COL_TEXT, COL_BG);
        _tft.setCursor(8, H - STATUS_H - 14);
        char msg[32];
        snprintf(msg, sizeof(msg), "%d-%d / %d  (enc1 scrolls)",
                 scrollOffset + 1, scrollOffset + visible, count);
        _tft.print(msg);
    }
}

// ---------------------------------------------------------------------------
// Roster helpers
// ---------------------------------------------------------------------------
void Display::drawHeader(const char *title, uint16_t bg, uint16_t fg) {
    _tft.fillRect(0, 0, W, ROSTER_HDR_H, bg);
    _tft.setTextColor(fg, bg);
    _tft.setTextSize(2);
    _tft.setCursor(8, ROSTER_HDR_H / 4);
    _tft.print(title);
}

void Display::drawStatusBar(bool connected) {
    int y = H - STATUS_H;
    _tft.fillRect(0, y, W, STATUS_H, COL_BG);
    _tft.setTextSize(1);
    if (connected) {
        _tft.setTextColor(TFT_GREEN, COL_BG);
        _tft.setCursor(8, y + 8);
        _tft.print("DCC-EX Connected");
    } else {
        _tft.setTextColor(TFT_RED, COL_BG);
        _tft.setCursor(8, y + 8);
        _tft.print("Connecting...");
    }
}

// ---------------------------------------------------------------------------
// Sleep / wake
// ---------------------------------------------------------------------------
void Display::sleep() {
#if TFT_BL_PIN >= 0
    digitalWrite(TFT_BL_PIN, LOW);
#endif
    _tft.writecommand(0x10);   // SLPIN
}

void Display::wake() {
    _tft.writecommand(0x11);   // SLPOUT
    delay(120);
#if TFT_BL_PIN >= 0
    digitalWrite(TFT_BL_PIN, HIGH);
#endif
}

// ---------------------------------------------------------------------------
// Touch (ST7796 / XPT2046 only)
// ---------------------------------------------------------------------------
#if DISPLAY_480
bool Display::getTouch(uint16_t &x, uint16_t &y) {
    return _tft.getTouch(&x, &y);
}

void Display::runCalibration(uint16_t* calData) {
    _tft.fillScreen(TFT_BLACK);
    _tft.setTextColor(TFT_WHITE, TFT_BLACK);
    _tft.setTextDatum(MC_DATUM);
    _tft.setFreeFont(&FreeSansBold12pt7b);
    _tft.drawString("Touch Calibration", W / 2, H / 2 - 30);
    _tft.setFreeFont(&FreeSans9pt7b);
    _tft.drawString("Tap each corner marker", W / 2, H / 2 + 10);
    _tft.drawString("Hold ROSTER at boot to recalibrate", W / 2, H / 2 + 40);
    _tft.setTextFont(1);
    _tft.setTextDatum(TL_DATUM);
    delay(2000);
    _tft.calibrateTouch(calData, TFT_WHITE, TFT_BLACK, 15);
}

void Display::applyCalibration(uint16_t* calData) {
    _tft.setTouch(calData);
}
#endif
