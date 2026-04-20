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
static constexpr uint16_t DIAL_BG      = TFT_WHITE;  // white dial face

// Chrome bezel layers
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
// Needle angles use 0° = 12 o'clock, clockwise (standard trig convention for TFT).
// TFT_eSPI drawSmoothArc uses 0° = 6 o'clock, clockwise.
// Conversion: tftArc = (needleAngle + 180) % 360
// ---------------------------------------------------------------------------
static constexpr int GAUGE_START = 220;   // needle convention
static constexpr int GAUGE_SWEEP = 280;

// Pre-converted arc start/end for drawSmoothArc
static constexpr uint32_t TFT_ARC_START = (GAUGE_START + 180) % 360;              // 40
static constexpr uint32_t TFT_ARC_END   = (GAUGE_START + GAUGE_SWEEP + 180) % 360; // 320

#if DISPLAY_480
static constexpr int GAUGE_R     = 100;
static constexpr int BEZEL_W     = 8;
static constexpr int GAUGE_THICK = 20;
static constexpr int GAUGE_CY    = 145;

static constexpr int ARC_R       = GAUGE_R - BEZEL_W;       // 92
static constexpr int ARC_IR      = ARC_R - GAUGE_THICK;     // 72

static constexpr int SPRITE_R    = ARC_R + 1;               // 93 — sprite half-size

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

static constexpr int SPRITE_R    = ARC_R + 1;               // 63

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

    // Allocate gauge face sprite once — reused for every column update
    _face.createSprite(2 * SPRITE_R, 2 * SPRITE_R);

    delay(2000);
}

// ---------------------------------------------------------------------------
// Gauge helpers
// ---------------------------------------------------------------------------

// Chrome bezel (static, drawn once per full column redraw; not part of sprite).
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

// Render arc ring, tick marks, needle and hub into _face sprite.
// The sprite uses TFT_TRANSPARENT for corner pixels outside the gauge circle,
// so pushFaceSprite skips those and the chrome bezel behind them is preserved.
//
// Arc angle fix: TFT_eSPI drawSmoothArc counts from 6 o'clock CW (0° = bottom).
// Our needle angles count from 12 o'clock CW.  Conversion: tft = (needle+180)%360.
void Display::renderFaceToSprite(int col, int speed) {
    static const char *labels[] = { "0", "25", "50", "76", "101", "126" };
    const int scx = SPRITE_R;
    const int scy = SPRITE_R;

    // Transparent background for corners; dial face fills the circular area
    _face.fillSprite(TFT_TRANSPARENT);
    _face.fillCircle(scx, scy, ARC_R + 1, DIAL_BG);

    // Arc ring — background (full sweep)
    _face.drawSmoothArc(scx, scy, ARC_R, ARC_IR,
                        TFT_ARC_START, TFT_ARC_END, COL_GAUGE_BG, DIAL_BG);

    // Arc ring — speed fill (partial sweep, corrected angles)
    float needleAngle = GAUGE_START + (float)speed / 126.0f * GAUGE_SWEEP;
    if (speed > 0) {
        uint32_t tftAngle = ((uint32_t)needleAngle + 180) % 360;
        _face.drawSmoothArc(scx, scy, ARC_R, ARC_IR,
                            TFT_ARC_START, tftAngle, COL_BAR[col & 3], DIAL_BG);
    }

    // Tick marks and speed labels (use needle-convention angles for x/y maths)
    for (int t = 0; t <= 20; t++) {
        float a    = (GAUGE_START + t * GAUGE_SWEEP / 20.0f) * DEG_TO_RAD;
        float sinA = sinf(a), cosA = cosf(a);
        bool  major = (t % 4 == 0);
        int   r2    = major ? TICK_MAJ_IN : TICK_MIN_IN;

        _face.drawLine(scx + (int)(TICK_OUTER * sinA), scy - (int)(TICK_OUTER * cosA),
                       scx + (int)(r2          * sinA), scy - (int)(r2          * cosA),
                       TFT_BLACK);

        if (major) {
            _face.setFreeFont(&FreeSans9pt7b);
            _face.setTextDatum(MC_DATUM);
            _face.setTextColor(TFT_BLACK, DIAL_BG);
            _face.drawString(labels[t / 4],
                             scx + (int)(LABEL_R * sinA),
                             scy - (int)(LABEL_R * cosA));
        }
    }
    _face.setTextFont(1);
    _face.setTextDatum(TL_DATUM);

    // Needle (needle-convention angle → correct screen direction)
    float rad = needleAngle * DEG_TO_RAD;
    int   nx  = scx + (int)(NEEDLE_R * sinf(rad));
    int   ny  = scy - (int)(NEEDLE_R * cosf(rad));
    _face.drawWideLine(scx, scy, nx, ny, NEEDLE_W, COL_NEEDLE, COL_NEEDLE);

    // Chrome hub cap
    _face.fillCircle(scx, scy, HUB_R,     CHR_BRIGHT);
    _face.fillCircle(scx, scy, HUB_R - 2, CHR_DARK);
    _face.fillCircle(scx, scy, HUB_R - 4, CHR_MED);
}

// Push sprite to screen, skipping transparent corner pixels so the chrome
// bezel drawn by drawBezelAndDial underneath is preserved.
void Display::pushFaceSprite(int col) {
    int cx = col * COL_W + COL_W / 2;
    _face.pushSprite(cx - SPRITE_R, GAUGE_CY - SPRITE_R, TFT_TRANSPARENT);
}

// Speed number and FWD/REV label drawn to screen below the gauge.
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
// Single column full redraw: header + static bezel + gauge face + texts
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

    // Gauge: static bezel drawn to screen, dynamic face rendered as sprite
    drawBezelAndDial(col);
    renderFaceToSprite(col, loco.speed);
    pushFaceSprite(col);
    drawGaugeTexts(col, loco);
}

// ---------------------------------------------------------------------------
// Speed-only update — render full gauge face into sprite and push atomically.
// The sprite replaces the dial area in one SPI burst; no partial state visible.
// ---------------------------------------------------------------------------
void Display::drawThrottleSpeed(int col, const LocoState &loco) {
    renderFaceToSprite(col, loco.speed);
    pushFaceSprite(col);

    // Update speed number only (direction text unchanged)
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
