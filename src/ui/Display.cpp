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
static constexpr int GAUGE_START = 240;   // needle convention (8 o'clock)
static constexpr int GAUGE_SWEEP = 240;   // to 4 o'clock

// Pre-converted arc start/end for drawSmoothArc
static constexpr uint32_t TFT_ARC_START = (GAUGE_START + 180) % 360;              // 40
static constexpr uint32_t TFT_ARC_END   = (GAUGE_START + GAUGE_SWEEP + 180) % 360; // 320

#if DISPLAY_480
static constexpr int GAUGE_R     = 100;
static constexpr int BEZEL_W     = 8;
static constexpr int GAUGE_THICK = 20;
static constexpr int GAUGE_CY    = 165;

static constexpr int ARC_R       = GAUGE_R - BEZEL_W;       // 92
static constexpr int ARC_IR      = ARC_R - GAUGE_THICK;     // 72

static constexpr int SPRITE_R    = ARC_R + 1;               // 93 — sprite half-size

static constexpr int NEEDLE_R    = 78;
static constexpr int NEEDLE_W    = 4;
static constexpr int HUB_R       = 10;

static constexpr int TICK_OUTER  = ARC_IR - 1;              // 71
static constexpr int TICK_MAJ_IN = ARC_IR - 17;             // 55
static constexpr int TICK_MIN_IN = ARC_IR - 9;              // 63
static constexpr int DIR_INNER_Y = 30;   // FWD/REV offset below sprite centre
static constexpr int SPD_INNER_Y = 65;   // speed number offset below sprite centre
static constexpr int SPD_BOX_W   = 72;
static constexpr int SPD_BOX_H   = 36;

static constexpr int ROSTER_HDR_H = 48;
static constexpr int ROSTER_ROW_H = 36;
static constexpr int STATUS_H     = 28;

// Header rows: top bar holds buttons+mA, col sub-header holds addresses
static constexpr int TOP_ROW_H  = 36;   // button/mA row height
static constexpr int COL_HDR_H  = HDR_H - TOP_ROW_H;  // 24 — address sub-header

// Top bar button zones (x ranges, y always 0..TOP_ROW_H)
static constexpr int PWR_BTN_X  = 2;
static constexpr int PWR_BTN_W  = 92;
static constexpr int STOP_BTN_X = 98;
static constexpr int STOP_BTN_W = 72;
static constexpr int MA_ZONE_X  = W - 88;  // mA text zone start
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
static constexpr int DIR_INNER_Y = 20;
static constexpr int SPD_INNER_Y = 42;
static constexpr int SPD_BOX_W   = 50;
static constexpr int SPD_BOX_H   = 24;

static constexpr int ROSTER_HDR_H = 36;
static constexpr int ROSTER_ROW_H = 32;
static constexpr int STATUS_H     = 24;
#endif

// ---------------------------------------------------------------------------
static constexpr int BL_LEDC_CH = 0;   // LEDC channel for backlight PWM

void Display::begin() {
#if TFT_BL_PIN >= 0
    ledcSetup(BL_LEDC_CH, 5000, 8);    // 5 kHz, 8-bit resolution
    ledcAttachPin(TFT_BL_PIN, BL_LEDC_CH);
    ledcWrite(BL_LEDC_CH, 255);
#endif
    _tft.init();
    _tft.setRotation(1);
    _tft.pushImage(0, 0, SCREEN_W, SCREEN_H, splash_pixels);

    // Gauge face sprite — reused for every column update
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
void Display::renderFaceToSprite(int col, const LocoState &loco) {
    const int scx = SPRITE_R;
    const int scy = SPRITE_R;
    const int speed = loco.speed;

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

    // Tick marks only — no speed labels (speed shown as number inside gauge)
    for (int t = 0; t <= 20; t++) {
        float a    = (GAUGE_START + t * GAUGE_SWEEP / 20.0f) * DEG_TO_RAD;
        float sinA = sinf(a), cosA = cosf(a);
        bool  major = (t % 4 == 0);
        int   r2    = major ? TICK_MAJ_IN : TICK_MIN_IN;

        _face.drawLine(scx + (int)(TICK_OUTER * sinA), scy - (int)(TICK_OUTER * cosA),
                       scx + (int)(r2          * sinA), scy - (int)(r2          * cosA),
                       TFT_BLACK);
    }

    // FWD/REV label above speed box
    _face.setTextDatum(MC_DATUM);
    _face.setFreeFont(&FreeSansBold9pt7b);
    _face.setTextColor(loco.forward ? COL_FWD : COL_REV, DIAL_BG);
    _face.drawString(loco.forward ? "FWD" : "REV", scx, scy + DIR_INNER_Y);

    // Speed box: fixed black background, white text, 0-100 scale
    int displaySpeed = (speed * 100 + 63) / 126;
    char spd[4];
    snprintf(spd, sizeof(spd), "%d", displaySpeed);
    _face.fillRoundRect(scx - SPD_BOX_W / 2, scy + SPD_INNER_Y - SPD_BOX_H / 2,
                        SPD_BOX_W, SPD_BOX_H, 4, TFT_BLACK);
#if DISPLAY_480
    _face.setFreeFont(&FreeSansBold18pt7b);
#else
    _face.setFreeFont(&FreeSans9pt7b);
#endif
    _face.setTextColor(TFT_WHITE);
    _face.drawString(spd, scx, scy + SPD_INNER_Y - 2);
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

// ---------------------------------------------------------------------------
// Top bar button zone (x=0..MA_ZONE_X only) — never touches the mA zone.
// drawCurrentReading owns the right portion independently.
// ---------------------------------------------------------------------------
void Display::drawTopBar(bool trackPower) {
    // Clear button area only — mA zone (x >= MA_ZONE_X) is untouched
    _tft.fillRect(0, 0, MA_ZONE_X, TOP_ROW_H, COL_HEADER);

    uint16_t pwrBg = trackPower ? TFT_GREEN : TFT_RED;
    _tft.fillRoundRect(PWR_BTN_X, 2, PWR_BTN_W, TOP_ROW_H - 4, 4, pwrBg);
    _tft.setFreeFont(&FreeSansBold9pt7b);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(TFT_WHITE, pwrBg);
    _tft.drawString(trackPower ? "PWR ON" : "PWR OFF",
                    PWR_BTN_X + PWR_BTN_W / 2, TOP_ROW_H / 2);

    _tft.fillRoundRect(STOP_BTN_X, 2, STOP_BTN_W, TOP_ROW_H - 4, 4, TFT_ORANGE);
    _tft.setTextColor(TFT_BLACK, TFT_ORANGE);
    _tft.drawString("STOP", STOP_BTN_X + STOP_BTN_W / 2, TOP_ROW_H / 2);

    _tft.setTextDatum(TL_DATUM);
}

// mA zone (x=MA_ZONE_X..W) — never touches the button area.
void Display::drawCurrentReading(int milliAmps) {
    _tft.fillRect(MA_ZONE_X, 0, W - MA_ZONE_X, TOP_ROW_H, COL_HEADER);
    if (milliAmps >= 0) {
        char ma[12];
        snprintf(ma, sizeof(ma), "%dmA", milliAmps);
        _tft.setFreeFont(&FreeSans9pt7b);
        _tft.setTextDatum(MR_DATUM);
        _tft.setTextColor(COL_TEXT, COL_HEADER);
        _tft.drawString(ma, W - 4, TOP_ROW_H / 2);
        _tft.setTextDatum(TL_DATUM);
    }
}

// Per-column address sub-header (y=TOP_ROW_H..HDR_H): aligned with gauge columns
void Display::drawColHeaders(bool connected, const LocoState *locos, int count) {
    _tft.fillRect(0, TOP_ROW_H, W, COL_HDR_H, COL_HEADER);
    _tft.setFreeFont(&FreeSans9pt7b);
    _tft.setTextDatum(MC_DATUM);
    for (int i = 0; i < count && i < NUM_THROTTLES; i++) {
        int cx = i * COL_W + COL_W / 2;
        int cy = TOP_ROW_H + COL_HDR_H / 2;
        char addr[8];
        snprintf(addr, sizeof(addr), "#%d", locos[i].address);
        // No bg in setTextColor — fillRect above already provides background.
        // Passing a bg color causes FreeFont to fill the full yAdvance cell height,
        // which overflows below the sub-header rect and produces stray coloured pixels.
        _tft.setTextColor(COL_TEXT);
        _tft.drawString(addr, cx - 6, cy);
        _tft.fillCircle(cx + _tft.textWidth(addr) / 2 + 2, cy, 4,
                        connected ? TFT_GREEN : TFT_RED);
    }
    // Clip any pixel that still bled below the sub-header rect, then draw divider
    _tft.fillRect(0, HDR_H, W, 1, COL_BG);
    // Column divider in sub-header row
    for (int i = 1; i < NUM_THROTTLES; i++)
        _tft.drawFastVLine(i * COL_W, TOP_ROW_H, COL_HDR_H, COL_DIVIDER);
    _tft.drawFastHLine(0, HDR_H, W, COL_DIVIDER);
    _tft.setTextDatum(TL_DATUM);
}


// ---------------------------------------------------------------------------
// Throttle screen — full redraw of all columns
// ---------------------------------------------------------------------------
void Display::drawThrottleScreen(const LocoState *locos, int count, bool connected,
                                 bool trackPower, int milliAmps) {
    // No fillScreen — per-column fills cover the whole body area (saves ~120ms at 20MHz SPI).
    // Draw columns first, then header rows on top, then divider last so it's not overwritten.
    for (int i = 0; i < count && i < NUM_THROTTLES; i++)
        drawThrottleColumn(i, locos[i], connected);
    drawTopBar(trackPower);
    drawCurrentReading(milliAmps);
    drawColHeaders(connected, locos, count);
    for (int i = 1; i < NUM_THROTTLES; i++)
        _tft.drawFastVLine(i * COL_W, HDR_H, H - HDR_H, COL_DIVIDER);
}

// ---------------------------------------------------------------------------
// Single column full redraw: static bezel + gauge face (no per-col header)
// ---------------------------------------------------------------------------
void Display::drawThrottleColumn(int col, const LocoState &loco, bool connected) {
    int x = col * COL_W;
    // Fill full COL_W (not COL_W-1) so adjacent columns together cover every pixel
    // without needing a fillScreen.  Divider is redrawn on top by drawThrottleScreen.
    _tft.fillRect(x, HDR_H + 1, COL_W, H - HDR_H - 1, COL_BG);
    drawBezelAndDial(col);
    renderFaceToSprite(col, loco);
    pushFaceSprite(col);
}

// ---------------------------------------------------------------------------
// Speed-only update — render full gauge face into sprite and push atomically.
// The sprite replaces the dial area in one SPI burst; no partial state visible.
// ---------------------------------------------------------------------------
void Display::drawThrottleSpeed(int col, const LocoState &loco) {
    renderFaceToSprite(col, loco);
    pushFaceSprite(col);
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


// ---------------------------------------------------------------------------
// Top bar hit test (ST7796 only)
// ---------------------------------------------------------------------------
#if DISPLAY_480
TopBarZone Display::hitTestTopBar(uint16_t tx, uint16_t ty) {
    if (ty >= (uint16_t)TOP_ROW_H) return TopBarZone::NONE;  // sub-header row: no buttons
    if (tx >= PWR_BTN_X && tx < PWR_BTN_X + PWR_BTN_W)    return TopBarZone::POWER_BTN;
    if (tx >= STOP_BTN_X && tx < STOP_BTN_X + STOP_BTN_W) return TopBarZone::STOP_BTN;
    return TopBarZone::NONE;
}
#endif

// ---------------------------------------------------------------------------
// Sleep / wake
// ---------------------------------------------------------------------------
void Display::sleep() {
#if TFT_BL_PIN >= 0
    for (int v = 255; v >= 0; v -= BL_FADE_STEP) { ledcWrite(BL_LEDC_CH, v); delay(BL_FADE_OUT_MS); }
    ledcWrite(BL_LEDC_CH, 0);
#endif
    _tft.writecommand(0x10);   // SLPIN
}

void Display::wake() {
    _tft.writecommand(0x11);   // SLPOUT
    delay(120);                // display IC mandatory settle time; backlight stays off
}

void Display::fadeInBacklight() {
#if TFT_BL_PIN >= 0
    for (int v = 0; v <= 255; v += BL_FADE_STEP) { ledcWrite(BL_LEDC_CH, v); delay(BL_FADE_IN_MS); }
    ledcWrite(BL_LEDC_CH, 255);
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
