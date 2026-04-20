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
static constexpr uint16_t COL_GAUGE_BG = 0x2104;
static constexpr uint16_t COL_DIVIDER  = 0x4208;
static constexpr uint16_t COL_SELECTED = 0x1E3F;
static constexpr uint16_t COL_NEEDLE   = TFT_RED;
static constexpr uint16_t DIAL_BG      = 0x1082;   // charcoal dial face

// Chrome bezel layers: dark shadow → bright highlight → mid → bright → dark
static constexpr uint16_t CHR_DARK   = 0x2104;
static constexpr uint16_t CHR_MED    = 0x738E;
static constexpr uint16_t CHR_BRIGHT = 0xEF7D;

// Per-throttle arc fill colours
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
static constexpr int HDR_H = Display::HDR_H;

// ---------------------------------------------------------------------------
// Gauge layout
// Angles: TFT_eSPI convention 0=12 o'clock, CW.
// Arc sweeps from 8 o'clock (220°) to 4 o'clock (220+280=500° ≡ 140°).
// ---------------------------------------------------------------------------
static constexpr int GAUGE_START = 220;
static constexpr int GAUGE_SWEEP = 280;

#if DISPLAY_480
static constexpr int GAUGE_R     = 100;  // outer bezel radius
static constexpr int BEZEL_W     = 8;    // chrome ring width
static constexpr int GAUGE_THICK = 20;   // arc ring thickness
static constexpr int GAUGE_CY    = 145;  // gauge centre y

// ARC_R = GAUGE_R - BEZEL_W (arc drawn inside bezel)
static constexpr int ARC_R       = GAUGE_R - BEZEL_W;       // 92
static constexpr int ARC_IR      = ARC_R - GAUGE_THICK;     // 72

static constexpr int NEEDLE_R    = 78;
static constexpr int NEEDLE_W    = 4;
static constexpr int HUB_R       = 10;

static constexpr int TICK_OUTER  = ARC_IR - 1;              // 71
static constexpr int TICK_MAJ_IN = ARC_IR - 17;             // 55
static constexpr int TICK_MIN_IN = ARC_IR - 9;              // 63
static constexpr int LABEL_R     = ARC_IR - 27;             // 45

static constexpr int GAUGE_SPD_Y = 262;
static constexpr int GAUGE_DIR_Y = 290;
static constexpr int SPD_BOX_W   = 100;
static constexpr int SPD_BOX_H   = 38;
static constexpr int DIR_BOX_W   = 80;
static constexpr int DIR_BOX_H   = 30;

static constexpr int ROSTER_HDR_H = 48;
static constexpr int ROSTER_ROW_H = 36;
static constexpr int STATUS_H     = 28;
#else
static constexpr int GAUGE_R     = 68;
static constexpr int BEZEL_W     = 6;
static constexpr int GAUGE_THICK = 14;
static constexpr int GAUGE_CY    = 100;

static constexpr int ARC_R       = GAUGE_R - BEZEL_W;       // 62
static constexpr int ARC_IR      = ARC_R - GAUGE_THICK;     // 48

static constexpr int NEEDLE_R    = 52;
static constexpr int NEEDLE_W    = 3;
static constexpr int HUB_R       = 7;

static constexpr int TICK_OUTER  = ARC_IR - 1;              // 47
static constexpr int TICK_MAJ_IN = ARC_IR - 12;             // 36
static constexpr int TICK_MIN_IN = ARC_IR - 6;              // 42
static constexpr int LABEL_R     = ARC_IR - 20;             // 28

static constexpr int GAUGE_SPD_Y = 185;
static constexpr int GAUGE_DIR_Y = 210;
static constexpr int SPD_BOX_W   = 80;
static constexpr int SPD_BOX_H   = 30;
static constexpr int DIR_BOX_W   = 64;
static constexpr int DIR_BOX_H   = 24;

static constexpr int ROSTER_HDR_H = 36;
static constexpr int ROSTER_ROW_H = 32;
static constexpr int STATUS_H     = 24;
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
// Gauge layer helpers
// ---------------------------------------------------------------------------

// Chrome bezel (concentric rings dark→bright→mid→bright→dark) + charcoal dial face.
// Pattern: outermost 1px dark, 1px bright, (BEZEL_W-4) px mid, 1px bright, 1px dark,
// then dial face fills the remainder.  Works for any BEZEL_W >= 4.
void Display::drawBezelAndDial(int col) {
    int cx = col * COL_W + COL_W / 2;
    int cy = GAUGE_CY;
    _tft.fillCircle(cx, cy, GAUGE_R,               CHR_DARK);
    _tft.fillCircle(cx, cy, GAUGE_R - 1,           CHR_BRIGHT);
    _tft.fillCircle(cx, cy, GAUGE_R - 2,           CHR_MED);
    _tft.fillCircle(cx, cy, GAUGE_R - BEZEL_W + 2, CHR_BRIGHT);
    _tft.fillCircle(cx, cy, GAUGE_R - BEZEL_W + 1, CHR_DARK);
    _tft.fillCircle(cx, cy, GAUGE_R - BEZEL_W,     DIAL_BG);
}

// 21 tick positions over 280° (14° per step).
// Major ticks every 4 steps (positions 0,4,8,12,16,20 = labels 0,25,50,76,101,126).
void Display::drawGaugeTicks(int col) {
    int cx = col * COL_W + COL_W / 2;
    int cy = GAUGE_CY;

    static const char *labels[] = { "0", "25", "50", "76", "101", "126" };

    for (int t = 0; t <= 20; t++) {
        float angle = (GAUGE_START + t * GAUGE_SWEEP / 20.0f) * DEG_TO_RAD;
        float sinA  = sinf(angle);
        float cosA  = cosf(angle);
        bool  major = (t % 4 == 0);
        int   r2    = major ? TICK_MAJ_IN : TICK_MIN_IN;

        _tft.drawLine(cx + (int)(TICK_OUTER * sinA), cy - (int)(TICK_OUTER * cosA),
                      cx + (int)(r2          * sinA), cy - (int)(r2          * cosA),
                      COL_TEXT);

        if (major) {
            int lx = cx + (int)(LABEL_R * sinA);
            int ly = cy - (int)(LABEL_R * cosA);
            _tft.setTextFont(1);
            _tft.setTextDatum(MC_DATUM);
            _tft.setTextColor(COL_TEXT, DIAL_BG);
            _tft.drawString(labels[t / 4], lx, ly);
        }
    }
    _tft.setTextDatum(TL_DATUM);
}

// Background arc (full sweep, dark) + filled speed arc.
// Also stores the new needle angle in _gaugeAngle[col].
void Display::drawGaugeArc(int col, int speed) {
    int cx = col * COL_W + COL_W / 2;
    int cy = GAUGE_CY;

    _tft.drawSmoothArc(cx, cy, ARC_R, ARC_IR,
                        GAUGE_START, GAUGE_START + GAUGE_SWEEP,
                        COL_GAUGE_BG, DIAL_BG);

    float angle = GAUGE_START + (float)speed / 126.0f * GAUGE_SWEEP;
    _gaugeAngle[col] = angle;

    if (speed > 0) {
        _tft.drawSmoothArc(cx, cy, ARC_R, ARC_IR,
                            GAUGE_START, (uint32_t)angle,
                            COL_BAR[col & 3], DIAL_BG);
    }
}

// Draw (or erase) a single needle at angleDeg using the given colour.
void Display::drawGaugeNeedle(int col, float angleDeg, uint16_t color) {
    int   cx  = col * COL_W + COL_W / 2;
    float rad = angleDeg * DEG_TO_RAD;
    int   nx  = cx + (int)(NEEDLE_R * sinf(rad));
    int   ny  = GAUGE_CY - (int)(NEEDLE_R * cosf(rad));
    _tft.drawWideLine(cx, GAUGE_CY, nx, ny, NEEDLE_W, color, color);
}

// Chrome hub cap: bright outer ring, dark core, mid highlight.
void Display::drawGaugeHub(int col) {
    int cx = col * COL_W + COL_W / 2;
    int cy = GAUGE_CY;
    _tft.fillCircle(cx, cy, HUB_R,     CHR_BRIGHT);
    _tft.fillCircle(cx, cy, HUB_R - 2, CHR_DARK);
    _tft.fillCircle(cx, cy, HUB_R - 4, CHR_MED);
}

// Speed number and FWD/REV label drawn below the gauge bezel.
void Display::drawGaugeTexts(int col, const LocoState &loco) {
    int cx = col * COL_W + COL_W / 2;

    _tft.fillRect(cx - SPD_BOX_W / 2, GAUGE_SPD_Y - SPD_BOX_H / 2,
                  SPD_BOX_W, SPD_BOX_H, COL_BG);
    _tft.setFreeFont(&FreeSansBold18pt7b);
    _tft.setTextColor(COL_TEXT);
    _tft.setTextDatum(MC_DATUM);
    char spd[4];
    snprintf(spd, sizeof(spd), "%d", loco.speed);
    _tft.drawString(spd, cx, GAUGE_SPD_Y);

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
// Single column full redraw: header + gauge layers + texts
// ---------------------------------------------------------------------------
void Display::drawThrottleColumn(int col, const LocoState &loco, bool connected) {
    int x = col * COL_W;

    _tft.fillRect(x, 0, COL_W - 1, H, COL_BG);

    // Header bar
    _tft.fillRect(x, 0, COL_W - 1, HDR_H, COL_HEADER);
    _tft.fillCircle(x + COL_W - 11, HDR_H / 2, 4, connected ? TFT_GREEN : TFT_RED);
    _tft.setFreeFont(&FreeSans9pt7b);
    _tft.setTextColor(COL_TEXT);
    _tft.setTextDatum(MC_DATUM);
    char addr[8];
    snprintf(addr, sizeof(addr), "#%d", loco.address);
    _tft.drawString(addr, x + (COL_W - 1 - 18) / 2, HDR_H / 2);
    _tft.drawFastHLine(x, HDR_H, COL_W - 1, COL_DIVIDER);

    // Gauge — draw layers in order
    drawBezelAndDial(col);
    drawGaugeArc(col, loco.speed);    // sets _gaugeAngle[col]
    drawGaugeTicks(col);
    drawGaugeNeedle(col, _gaugeAngle[col], COL_NEEDLE);
    drawGaugeHub(col);
    drawGaugeTexts(col, loco);
}

// ---------------------------------------------------------------------------
// Speed-only update — flicker-free
// Erase old needle → redraw arc → redraw ticks → draw new needle → update text
// ---------------------------------------------------------------------------
void Display::drawThrottleSpeed(int col, const LocoState &loco) {
    // 1. Erase old needle by drawing over it in dial colour
    drawGaugeNeedle(col, _gaugeAngle[col], DIAL_BG);

    // 2. Redraw arcs (updates _gaugeAngle[col] to new angle)
    drawGaugeArc(col, loco.speed);

    // 3. Redraw ticks (arc can overwrite tick pixels at arc boundaries)
    drawGaugeTicks(col);

    // 4. New needle + hub
    drawGaugeNeedle(col, _gaugeAngle[col], COL_NEEDLE);
    drawGaugeHub(col);

    // 5. Speed number only (direction unchanged, skip direction box)
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
        int      idx   = scrollOffset + i;
        bool     hi    = (entries[idx].address == DEFAULT_LOCO_ADDRESS);
        uint16_t rowBg = hi ? COL_SELECTED : COL_BG;

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
