#include "Display.h"
#include "../config.h"
#if DISPLAY_480
#include "fn_icons.h"
#endif

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

// Function button colours
static constexpr uint16_t COL_FN_ACTIVE   = 0x03E0;  // TFT_DARKGREEN
static constexpr uint16_t COL_FN_INACTIVE = 0x39C7;  // medium dark grey
static constexpr uint16_t COL_FN_TXT_ON   = TFT_WHITE;
static constexpr uint16_t COL_FN_TXT_OFF  = 0xC618;  // light grey

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
static constexpr int GAUGE_R     = 92;
static constexpr int BEZEL_W     = 7;
static constexpr int GAUGE_THICK = 18;
static constexpr int GAUGE_CY    = 177;

static constexpr int ARC_R       = GAUGE_R - BEZEL_W;       // 85
static constexpr int ARC_IR      = ARC_R - GAUGE_THICK;     // 67

static constexpr int SPRITE_R    = GAUGE_R + 1;              // 93 — full bezel in sprite

static constexpr int NEEDLE_R    = 72;
static constexpr int NEEDLE_W    = 4;
static constexpr int HUB_R       = 9;

static constexpr int TICK_OUTER  = ARC_IR - 1;              // 66
static constexpr int TICK_MAJ_IN = ARC_IR - 15;             // 52
static constexpr int TICK_MIN_IN = ARC_IR - 8;              // 59
static constexpr int DIR_INNER_Y = 27;   // FWD/REV offset below sprite centre
static constexpr int SPD_INNER_Y = 59;   // speed number offset below sprite centre
static constexpr int SPD_BOX_W   = 66;
static constexpr int SPD_BOX_H   = 32;

static constexpr int ROSTER_ROW_H = 36;
static constexpr int STATUS_H     = 28;

// Header rows: top bar holds buttons+mA, col sub-header holds addresses
static constexpr int TOP_ROW_H  = 40;   // button/mA row height
static constexpr int COL_HDR_H  = HDR_H - TOP_ROW_H;  // 36 — address sub-header

// Function screen strip (gap below gauge face on throttle screen)
static constexpr int FN_STRIP_Y = Display::FN_STRIP_Y;   // 278
static constexpr int FN_STRIP_H = H - FN_STRIP_Y;        // 42
static constexpr int FN_COLS    = Display::FN_COLS;       // 4
static constexpr int FN_ROWS    = Display::FN_ROWS;       // 5
static constexpr int FN_BTN_W   = Display::FN_BTN_W;     // 120
static constexpr int FN_BTN_H   = Display::FN_BTN_H;     // 48
static constexpr int FN_PAD     = 3;

// Top bar layout: equal-width PWR button (left) | mA reading (centre) | STOP button (right)
static constexpr int BTN_W      = 96;
static constexpr int BTN_MARGIN = 4;
static constexpr int PWR_BTN_X  = BTN_MARGIN;                       // 4
static constexpr int STOP_BTN_X = W - BTN_W - BTN_MARGIN;           // 380
static constexpr int MA_START   = PWR_BTN_X + BTN_W + BTN_MARGIN;   // 104  (centre zone left)
static constexpr int MA_END     = STOP_BTN_X - BTN_MARGIN;          // 376  (centre zone right)
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

    // Transparent corners; chrome bezel then white dial — all rendered atomically in sprite
    _face.fillSprite(TFT_TRANSPARENT);
    _face.fillCircle(scx, scy, GAUGE_R,               CHR_DARK);
    _face.fillCircle(scx, scy, GAUGE_R - 1,           CHR_BRIGHT);
    _face.fillCircle(scx, scy, GAUGE_R - 2,           CHR_MED);
    _face.fillCircle(scx, scy, GAUGE_R - BEZEL_W + 2, CHR_BRIGHT);
    _face.fillCircle(scx, scy, GAUGE_R - BEZEL_W + 1, CHR_DARK);
    _face.fillCircle(scx, scy, GAUGE_R - BEZEL_W,     DIAL_BG);

    // Arc ring — background (full sweep, dark)
    _face.drawSmoothArc(scx, scy, ARC_R, ARC_IR,
                        TFT_ARC_START, TFT_ARC_END, COL_GAUGE_BG, DIAL_BG);

    // Arc ring — four-zone speed fill:
    //   0–25 % → green,  25–50 % → yellow,  50–75 % → amber,  75–100 % → red
    // Zone boundaries in TFT arc angle (0° = 6 o'clock, CW):
    //   Z1 = SWEEP/4  → TFT 120°,  Z2 = SWEEP/2 → TFT 180°,  Z3 = 3*SWEEP/4 → TFT 240°
    static constexpr uint32_t ARC_Z1 = (GAUGE_START + GAUGE_SWEEP * 1 / 4 + 180) % 360; // 120
    static constexpr uint32_t ARC_Z2 = (GAUGE_START + GAUGE_SWEEP * 2 / 4 + 180) % 360; // 180
    static constexpr uint32_t ARC_Z3 = (GAUGE_START + GAUGE_SWEEP * 3 / 4 + 180) % 360; // 240
    static constexpr uint16_t ARC_AMBER = 0xFD00; // full red, half green, no blue

    float needleAngle = GAUGE_START + (float)speed / 126.0f * GAUGE_SWEEP;
    if (speed > 0) {
        uint32_t ta = ((uint32_t)needleAngle + 180) % 360;
        // Zone 1 — green
        _face.drawSmoothArc(scx, scy, ARC_R, ARC_IR,
                            TFT_ARC_START, ta < ARC_Z1 ? ta : ARC_Z1, TFT_GREEN, DIAL_BG);
        // Zone 2 — yellow
        if (ta > ARC_Z1)
            _face.drawSmoothArc(scx, scy, ARC_R, ARC_IR,
                                ARC_Z1, ta < ARC_Z2 ? ta : ARC_Z2, TFT_YELLOW, DIAL_BG);
        // Zone 3 — amber
        if (ta > ARC_Z2)
            _face.drawSmoothArc(scx, scy, ARC_R, ARC_IR,
                                ARC_Z2, ta < ARC_Z3 ? ta : ARC_Z3, ARC_AMBER, DIAL_BG);
        // Zone 4 — red
        if (ta > ARC_Z3)
            _face.drawSmoothArc(scx, scy, ARC_R, ARC_IR,
                                ARC_Z3, ta, TFT_RED, DIAL_BG);
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
// Top bar: PWR button (left) and STOP button (right). Centre zone untouched.
// ---------------------------------------------------------------------------
void Display::drawTopBar(bool trackPower) {
    // Fill left and right button zones only — centre (mA) is independent.
    _tft.fillRect(0,         0, MA_START,      TOP_ROW_H, COL_HEADER);
    _tft.fillRect(MA_END, 0, W - MA_END, TOP_ROW_H, COL_HEADER);

    uint16_t pwrBg = trackPower ? TFT_GREEN : TFT_RED;
    _tft.fillRoundRect(PWR_BTN_X, 2, BTN_W, TOP_ROW_H - 4, 4, pwrBg);
    _tft.setFreeFont(&FreeSansBold9pt7b);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(TFT_WHITE, pwrBg);
    _tft.drawString(trackPower ? "PWR ON" : "PWR OFF",
                    PWR_BTN_X + BTN_W / 2, TOP_ROW_H / 2);

    _tft.fillRoundRect(STOP_BTN_X, 2, BTN_W, TOP_ROW_H - 4, 4, TFT_ORANGE);
    _tft.setTextColor(TFT_BLACK, TFT_ORANGE);
    _tft.drawString("STOP", STOP_BTN_X + BTN_W / 2, TOP_ROW_H / 2);

    _tft.setTextDatum(TL_DATUM);
}

// Centre zone (mA reading) — never touches the button areas.
void Display::drawCurrentReading(int milliAmps) {
    _tft.fillRect(MA_START, 0, MA_END - MA_START, TOP_ROW_H, COL_HEADER);
    if (milliAmps >= 0) {
        char ma[16];
        snprintf(ma, sizeof(ma), "%d mA", milliAmps);   // space between value and unit
        _tft.setFreeFont(&FreeSans9pt7b);
        _tft.setTextDatum(MC_DATUM);
        _tft.setTextColor(COL_TEXT, COL_HEADER);
        _tft.drawString(ma, (MA_START + MA_END) / 2, TOP_ROW_H / 2);
        _tft.setTextDatum(TL_DATUM);
    }
}

// Per-column sub-header (y=TOP_ROW_H..HDR_H): "Name  #addr  ●"
// Dot right-anchored; address immediately left; name fills remaining space (truncated).
void Display::drawColHeaders(bool connected, const LocoState *locos, int count,
                             const char (*names)[32]) {
    _tft.fillRect(0, TOP_ROW_H, W, COL_HDR_H, COL_HEADER);
    _tft.setFreeFont(&FreeSans9pt7b);
    _tft.setTextDatum(ML_DATUM);

    const int dotR   = 5;
    const int gap    = 6;
    const int margin = 7;

    for (int i = 0; i < count && i < NUM_THROTTLES; i++) {
        int colL = i * COL_W;
        int cy   = TOP_ROW_H + COL_HDR_H / 2;

        char addrStr[8];
        snprintf(addrStr, sizeof(addrStr), "#%d", locos[i].address);
        int addrW = _tft.textWidth(addrStr);

        // Right-anchor: dot, then address to its left
        int dotCx = colL + COL_W - margin - dotR;
        int addrX = dotCx - dotR - gap - addrW;

        // Name in remaining left space
        int nameMaxW = addrX - gap - (colL + margin);
        char truncName[32] = {};
        if (names && names[i][0] && nameMaxW > 4) {
            strncpy(truncName, names[i], sizeof(truncName) - 1);
            while (truncName[0] && _tft.textWidth(truncName) > nameMaxW)
                truncName[strlen(truncName) - 1] = '\0';
        }

        if (truncName[0]) {
            _tft.setTextColor(COL_TEXT, COL_HEADER);
            _tft.drawString(truncName, colL + margin, cy);
        }
        _tft.setTextColor(COL_ACCENT, COL_HEADER);
        _tft.drawString(addrStr, addrX, cy);
        _tft.fillCircle(dotCx, cy, dotR, connected ? TFT_GREEN : TFT_RED);
    }

    for (int i = 1; i < NUM_THROTTLES; i++)
        _tft.drawFastVLine(i * COL_W, TOP_ROW_H, COL_HDR_H, COL_DIVIDER);
    _tft.drawFastHLine(0, HDR_H, W, COL_DIVIDER);
    _tft.setTextDatum(TL_DATUM);
}


// ---------------------------------------------------------------------------
// Throttle screen — full redraw of all columns
// Batch strategy: fill entire body first, then push all gauge sprites, then headers.
// Bezel layers are baked into the sprite so each push is one atomic SPI burst —
// no partial chrome ring is ever visible.
// ---------------------------------------------------------------------------
void Display::drawThrottleScreen(const LocoState *locos, int count, bool connected,
                                 bool trackPower, int milliAmps, const char (*names)[32]) {
    // One fillRect for the entire body eliminates per-column black flashes.
    _tft.fillRect(0, HDR_H + 1, W, H - HDR_H - 1, COL_BG);
    for (int i = 0; i < count && i < NUM_THROTTLES; i++) {
        renderFaceToSprite(i, locos[i]);
        pushFaceSprite(i);
    }
    drawTopBar(trackPower);
    drawCurrentReading(milliAmps);
    drawColHeaders(connected, locos, count, names);
    for (int i = 1; i < NUM_THROTTLES; i++)
        _tft.drawFastVLine(i * COL_W, HDR_H, H - HDR_H, COL_DIVIDER);

    // Mini function strip — caller (UITask) populates it via drawMiniFunctions()
    // after this returns, under functionMutex.
}

// ---------------------------------------------------------------------------
// Single column full redraw (used for direction-change redraws etc.)
// ---------------------------------------------------------------------------
void Display::drawThrottleColumn(int col, const LocoState &loco, bool connected) {
    _tft.fillRect(col * COL_W, HDR_H + 1, COL_W, H - HDR_H - 1, COL_BG);
    renderFaceToSprite(col, loco);
    pushFaceSprite(col);
    // Mini function strip — caller populates it via drawMiniFunctions() under functionMutex.
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
// Rows start at HDR_H so touch zones match the throttle screen zones:
//   ty < TOP_BAR_H  → button row (PWR/STOP still work)
//   TOP_BAR_H ≤ ty < HDR_H  → sub-header "tap to go back"
//   ty ≥ HDR_H  → row tap → assign loco to throttle slot
// ---------------------------------------------------------------------------
void Display::drawRosterScreen(const RosterEntry *entries, int count, int scrollOffset,
                               int highlightAddr, bool received) {
    _tft.fillScreen(COL_BG);

    // Full-height header bar (tap anywhere → back to throttle)
    _tft.fillRect(0, 0, W, HDR_H, COL_HEADER);

    // "ROSTER" title, left-aligned
    _tft.setFreeFont(&FreeSansBold9pt7b);
    _tft.setTextDatum(ML_DATUM);
    _tft.setTextColor(COL_TEXT, COL_HEADER);
    _tft.drawString("ROSTER", 14, HDR_H / 2);

    // Back arrow icon (left-pointing solid triangle) on the right
    {
        int cx = W - 30, cy = HDR_H / 2, hw = 12, hv = 14;
        _tft.fillTriangle(cx - hw, cy, cx + hw, cy - hv, cx + hw, cy + hv, COL_TEXT);
    }

    _tft.drawFastHLine(0, HDR_H, W, COL_DIVIDER);

    if (count == 0) {
        _tft.setTextDatum(MC_DATUM);
        _tft.setTextColor(COL_TEXT, COL_BG);
        _tft.drawString(received ? "CS roster is empty" : "Waiting for CS roster...",
                        W / 2, HDR_H + (H - HDR_H) / 2);
        _tft.setTextFont(1);
        _tft.setTextDatum(TL_DATUM);
        return;
    }

    int visible = min(ROSTER_VISIBLE_ROWS, count - scrollOffset);
    for (int i = 0; i < visible; i++) {
        int      idx   = scrollOffset + i;
        bool     hi    = (entries[idx].address == highlightAddr);
        uint16_t rowBg = hi ? COL_SELECTED : (i & 1 ? COL_HEADER : COL_BG);
        int      rowY  = HDR_H + i * ROSTER_ROW_H;

        _tft.fillRect(0, rowY, W, ROSTER_ROW_H - 1, rowBg);
        _tft.drawFastHLine(0, rowY + ROSTER_ROW_H - 1, W, COL_DIVIDER);

        char addr[8];
        snprintf(addr, sizeof(addr), "#%d", entries[idx].address);
        _tft.setFreeFont(&FreeSansBold9pt7b);
        _tft.setTextDatum(ML_DATUM);
        _tft.setTextColor(COL_ACCENT, rowBg);
        _tft.drawString(addr, 8, rowY + ROSTER_ROW_H / 2);

        _tft.setFreeFont(&FreeSans9pt7b);
        _tft.setTextColor(COL_TEXT, rowBg);
        _tft.drawString(entries[idx].name[0] ? entries[idx].name : "(unnamed)",
                        80, rowY + ROSTER_ROW_H / 2);
    }

    // Status bar below rows
    _tft.setFreeFont(&FreeSans9pt7b);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(0x8410, COL_BG);
    int statusY = HDR_H + visible * ROSTER_ROW_H;
    if (count > ROSTER_VISIBLE_ROWS) {
        char msg[24];
        snprintf(msg, sizeof(msg), "%d-%d / %d",
                 scrollOffset + 1, scrollOffset + visible, count);
        _tft.drawString(msg, W / 2, statusY + 14);
    }
    _tft.drawString("tap row to assign loco", W / 2, H - 10);

    _tft.setTextFont(1);
    _tft.setTextDatum(TL_DATUM);
}


// ---------------------------------------------------------------------------
// Function icon renderer (ST7796 only)
// Draws a geometric icon at (cx, cy) to represent the given function.
// Matches on case-insensitive substrings of the function name; falls back to
// "F#" text for unrecognised names.  All drawing goes directly to _tft.
// ---------------------------------------------------------------------------
#if DISPLAY_480
void Display::drawFnIcon(const char *name, int fnNum,
                          uint16_t color, uint16_t bg, int cx, int cy) {
    char n[FUNC_NAME_LEN + 4] = {};
    if (name)
        for (int i = 0; name[i] && i < (int)sizeof(n) - 1; i++)
            n[i] = (char)tolower((unsigned char)name[i]);

    auto has = [&](const char *s) -> bool {
        return n[0] && strstr(n, s) != nullptr;
    };

    // cab/inter checked first so "Cab Light" doesn't fall through to the headlight case
    if (has("cab") || has("inter") || has("dim") || has("numb")) {
        // Interior / cab light: smaller sun with 6 rays
        _tft.fillCircle(cx, cy, 5, color);
        for (int a = 0; a < 360; a += 60) {
            float r = a * DEG_TO_RAD;
            _tft.drawLine(cx + (int)(7  * sinf(r)), cy - (int)(7  * cosf(r)),
                          cx + (int)(11 * sinf(r)), cy - (int)(11 * cosf(r)), color);
        }
    } else if (has("bell")) {
        // Bell: filled triangle body + rim bar + clapper dot
        _tft.fillTriangle(cx, cy - 11, cx - 9, cy + 4, cx + 9, cy + 4, color);
        _tft.fillRect(cx - 9, cy + 4, 18, 3, color);
        _tft.fillCircle(cx, cy + 10, 3, color);
    } else if (has("horn") || has("whistle") || has("bugle")) {
        // horn checked before "high" so "High Horn" shows a horn, not a headlight
        _tft.fillRect(cx - 13, cy - 3, 6, 6, color);
        _tft.fillTriangle(cx - 7, cy - 10, cx - 7, cy + 10, cx + 10, cy, color);
    } else if (has("high") || has("full beam") || has("highbeam")) {
        _tft.drawBitmap(cx - 12, cy - 12, icon_car_light_high, 24, 24, color);
    } else if (has("light") || has("head") || has("ditch") || has("beam") || has("marker")) {
        _tft.drawBitmap(cx - 12, cy - 12, icon_car_light_dimmed, 24, 24, color);
    } else if (has("mute") || has("silent") || has("quiet")) {
        // Muted speaker: body + X cross
        _tft.fillRect(cx - 13, cy - 3, 5, 6, color);
        _tft.fillTriangle(cx - 8, cy - 7, cx - 8, cy + 7, cx + 1, cy, color);
        _tft.drawWideLine(cx + 4, cy - 6, cx + 11, cy + 6, 2, color, color);
        _tft.drawWideLine(cx + 4, cy + 6, cx + 11, cy - 6, 2, color, color);
    } else if (has("sound") || has("audio") || has("volume") || has("exhaust")) {
        // Speaker + arc (sound wave)
        _tft.fillRect(cx - 11, cy - 3, 5, 6, color);
        _tft.fillTriangle(cx - 6, cy - 7, cx - 6, cy + 7, cx + 3, cy, color);
        _tft.drawSmoothArc(cx + 8, cy, 7, 5, 180, 360, color, bg);
    } else if (has("brake") || has("brk") || has("dyn")) {
        // Brake disc: outer ring + centre hub
        _tft.drawSmoothArc(cx, cy, 12, 9, 0, 360, color, bg);
        _tft.fillCircle(cx, cy, 4, color);
    } else if (has("coupl") || has("knuckle") || has("uncouple")) {
        // Coupler: two interlocked D-shapes + top/bottom bars
        _tft.drawSmoothArc(cx - 5, cy, 7, 5, 0, 180, color, bg);
        _tft.drawSmoothArc(cx + 5, cy, 7, 5, 180, 360, color, bg);
        _tft.fillRect(cx - 5, cy - 7, 10, 2, color);
        _tft.fillRect(cx - 5, cy + 5, 10, 2, color);
    } else if (has("smoke") || has("steam") || has("vapor") || has("cloud")) {
        // Three rising wavy lines
        for (int i = -1; i <= 1; i++) {
            int x = cx + i * 6;
            _tft.drawLine(x,     cy + 7, x + 3,  cy + 1, color);
            _tft.drawLine(x + 3, cy + 1, x,      cy - 5, color);
            _tft.drawLine(x,     cy - 5, x + 3,  cy - 11, color);
        }
    } else if (has("fan") || has("blower") || has("cool")) {
        // Fan: outer ring + cross blades + hub
        _tft.drawSmoothArc(cx, cy, 12, 10, 0, 360, color, bg);
        _tft.drawLine(cx - 9, cy, cx + 9, cy, color);
        _tft.drawLine(cx, cy - 9, cx, cy + 9, color);
        _tft.fillCircle(cx, cy, 3, color);
    } else if (has("panto") || has("collect")) {
        // Pantograph: diamond + overhead wire
        _tft.drawLine(cx, cy - 11, cx - 9, cy, color);
        _tft.drawLine(cx, cy - 11, cx + 9, cy, color);
        _tft.drawLine(cx - 9, cy, cx, cy + 8, color);
        _tft.drawLine(cx + 9, cy, cx, cy + 8, color);
        _tft.drawFastHLine(cx - 13, cy - 13, 26, color);
    } else if (has("rev") || (has("back") && !has("light"))) {
        // Reverse arrow
        _tft.fillTriangle(cx - 12, cy, cx - 2, cy - 9, cx - 2, cy + 9, color);
        _tft.fillRect(cx - 2, cy - 4, 12, 8, color);
    } else if (has("fwd") || has("forward") || has("dir")) {
        // Forward arrow
        _tft.fillTriangle(cx + 12, cy, cx + 2, cy - 9, cx + 2, cy + 9, color);
        _tft.fillRect(cx - 10, cy - 4, 12, 8, color);
    } else {
        // Default: "F#" text
        _tft.setFreeFont(&FreeSans9pt7b);
        _tft.setTextDatum(MC_DATUM);
        _tft.setTextColor(color, bg);
        char txt[6];
        snprintf(txt, sizeof(txt), "F%d", fnNum);
        _tft.drawString(txt, cx, cy);
        _tft.setTextFont(1);
        _tft.setTextDatum(TL_DATUM);
    }
}
#endif

// ---------------------------------------------------------------------------
// Single-button redraw — no screen clear, no header touch.
// Caller must hold functionMutex.  No-op if fnNum is not in the current window.
// ---------------------------------------------------------------------------
#if DISPLAY_480
void Display::drawFnButton(int fnNum, const LocoFunctionData &funcData, int scroll) {
    int gridRow = fnNum / FN_COLS - scroll;
    int gridCol = fnNum % FN_COLS;
    if (gridRow < 0 || gridRow >= FN_ROWS) return;

    bool     active = funcData.valid && ((funcData.states >> fnNum) & 1);
    uint16_t bgCol  = active ? COL_FN_ACTIVE : COL_FN_INACTIVE;
    uint16_t txtCol = active ? COL_FN_TXT_ON : COL_FN_TXT_OFF;

    int bx = gridCol * FN_BTN_W + FN_PAD;
    int by = HDR_H + gridRow * FN_BTN_H + FN_PAD;
    int bw = FN_BTN_W - 2 * FN_PAD;
    int bh = FN_BTN_H - 2 * FN_PAD;

    _tft.fillRoundRect(bx, by, bw, bh, 5, bgCol);
    const char *iconName = (funcData.valid && funcData.defs[fnNum].name[0])
                           ? funcData.defs[fnNum].name : nullptr;
    drawFnIcon(iconName, fnNum, txtCol, bgCol, bx + bw / 2, by + bh / 2);
    if (funcData.valid && funcData.defs[fnNum].momentary)
        _tft.fillCircle(bx + bw - 5, by + 5, 3, COL_ACCENT);
}
#endif

// ---------------------------------------------------------------------------
// Function screen (ST7796 only)
// Caller must hold functionMutex for the duration.
// scroll: number of button rows scrolled (encoder-controlled).
// ---------------------------------------------------------------------------
#if DISPLAY_480
void Display::drawFunctionScreen(int slot, const LocoState &loco,
                                  const char *locoName,
                                  const LocoFunctionData &funcData, int scroll) {
    _tft.fillScreen(COL_BG);

    // Header bar
    _tft.fillRect(0, 0, W, HDR_H, COL_HEADER);

    // Title: "Fn: <name> #addr" left-aligned
    char title[52];
    if (locoName && locoName[0])
        snprintf(title, sizeof(title), "Fn: %s #%d", locoName, loco.address);
    else
        snprintf(title, sizeof(title), "Fn: #%d", loco.address);
    _tft.setFreeFont(&FreeSansBold9pt7b);
    _tft.setTextDatum(ML_DATUM);
    _tft.setTextColor(COL_TEXT, COL_HEADER);
    _tft.drawString(title, 14, HDR_H / 2);

    // Throttle slot indicator just left of back arrow
    char slotStr[4];
    snprintf(slotStr, sizeof(slotStr), "T%d", slot + 1);
    _tft.setTextColor(COL_ACCENT, COL_HEADER);
    _tft.setTextDatum(MR_DATUM);
    _tft.drawString(slotStr, W - 62, HDR_H / 2);

    // Back arrow (left-pointing filled triangle)
    {
        int cx = W - 30, cy = HDR_H / 2, hw = 12, hv = 14;
        _tft.fillTriangle(cx - hw, cy, cx + hw, cy - hv, cx + hw, cy + hv, COL_TEXT);
    }
    _tft.drawFastHLine(0, HDR_H, W, COL_DIVIDER);

    // Button grid — delegate each button to drawFnButton to share drawing logic
    for (int fn = scroll * FN_COLS;
         fn < (scroll + FN_ROWS) * FN_COLS && fn < MAX_LOCO_FUNCTIONS; fn++) {
        drawFnButton(fn, funcData, scroll);
    }

    // Scroll hint at bottom right if there are more functions
    int totalFns = MAX_LOCO_FUNCTIONS;
    int maxScroll = totalFns / FN_COLS - FN_ROWS;
    if (maxScroll > 0) {
        _tft.setFreeFont(&FreeSans9pt7b);
        _tft.setTextDatum(BR_DATUM);
        _tft.setTextColor(0x8410, COL_BG);
        char hint[20];
        snprintf(hint, sizeof(hint), "F%d–F%d  ↕",
                 scroll * FN_COLS, (scroll + FN_ROWS) * FN_COLS - 1);
        _tft.drawString(hint, W - 4, H - 2);
    }

    _tft.setTextFont(1);
    _tft.setTextDatum(TL_DATUM);
}
#endif

// ---------------------------------------------------------------------------
// Mini function strip — 3 fn buttons (F0/F1/F2 by roster order) + "+" per column.
// Caller must hold functionMutex.
// ---------------------------------------------------------------------------
#if DISPLAY_480
static constexpr int MINI_BTN_W = Display::MINI_BTN_W;  // 60

int Display::miniFnNum(const LocoFunctionData &funcData, int btnIdx) {
    if (!funcData.valid) return btnIdx;
    int count = 0;
    for (int f = 0; f < MAX_LOCO_FUNCTIONS; f++) {
        if (funcData.defs[f].name[0] != '\0') {
            if (count == btnIdx) return f;
            count++;
        }
    }
    return -1;
}

void Display::drawMiniFnButton(int col, int btnIdx, const LocoFunctionData &funcData) {
    const int pad = 2;
    int bx = col * COL_W + btnIdx * MINI_BTN_W + pad;
    int by = FN_STRIP_Y + pad;
    int bw = MINI_BTN_W - 2 * pad;
    int bh = FN_STRIP_H - 2 * pad;

    if (btnIdx == MINI_BTN_COUNT) {
        _tft.fillRoundRect(bx, by, bw, bh, 4, COL_HEADER);
        _tft.setFreeFont(&FreeSans9pt7b);
        _tft.setTextDatum(MC_DATUM);
        _tft.setTextColor(COL_TEXT, COL_HEADER);
        _tft.drawString("+", bx + bw / 2, by + bh / 2);
        _tft.setTextDatum(TL_DATUM);
    } else {
        int fnNum = miniFnNum(funcData, btnIdx);
        if (fnNum < 0) {
            _tft.fillRoundRect(bx, by, bw, bh, 4, COL_FN_INACTIVE);
            return;
        }
        bool active     = funcData.valid && ((funcData.states >> fnNum) & 1);
        uint16_t bgCol  = active ? COL_FN_ACTIVE  : COL_FN_INACTIVE;
        uint16_t txtCol = active ? COL_FN_TXT_ON  : COL_FN_TXT_OFF;
        _tft.fillRoundRect(bx, by, bw, bh, 4, bgCol);
        const char *iconName = (funcData.valid && funcData.defs[fnNum].name[0])
                               ? funcData.defs[fnNum].name : nullptr;
        drawFnIcon(iconName, fnNum, txtCol, bgCol, bx + bw / 2, by + bh / 2);
        if (funcData.valid && funcData.defs[fnNum].momentary)
            _tft.fillCircle(bx + bw - 4, by + 4, 3, COL_ACCENT);
    }
}

void Display::drawMiniFunctions(int col, const LocoFunctionData &funcData) {
    _tft.drawFastHLine(col * COL_W, FN_STRIP_Y, COL_W, COL_DIVIDER);
    for (int i = 0; i <= MINI_BTN_COUNT; i++)
        drawMiniFnButton(col, i, funcData);
}
#endif

// ---------------------------------------------------------------------------
// Top bar hit test (ST7796 only)
// ---------------------------------------------------------------------------
#if DISPLAY_480
TopBarZone Display::hitTestTopBar(uint16_t tx, uint16_t ty) {
    if (ty >= (uint16_t)TOP_ROW_H) return TopBarZone::NONE;
    if (tx >= (uint16_t)PWR_BTN_X  && tx < (uint16_t)(PWR_BTN_X  + BTN_W)) return TopBarZone::POWER_BTN;
    if (tx >= (uint16_t)STOP_BTN_X && tx < (uint16_t)(STOP_BTN_X + BTN_W)) return TopBarZone::STOP_BTN;
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
    // Software reset clears any register corruption (colour inversion, MADCTL, etc.)
    // then re-runs the full ST7796 init register sequence.  Backlight stays off throughout
    // so the user sees nothing during the ~300ms reinit + redraw that follows.
    _tft.writecommand(0x01);  delay(120);   // SWRST — mandatory 120ms settle
    _tft.writecommand(0x11);  delay(120);   // SLPOUT — mandatory 120ms settle

    // Restore all ST7796 registers (mirrors ST7796_Init.h, skipping non-mandatory delays)
    _tft.writecommand(0xF0); _tft.writedata(0xC3);   // enable extension cmd set 2 part I
    _tft.writecommand(0xF0); _tft.writedata(0x96);   // enable extension cmd set 2 part II
    _tft.writecommand(0x36); _tft.writedata(0x48);   // MADCTL (overridden by setRotation below)
    _tft.writecommand(0x3A); _tft.writedata(0x55);   // COLMOD: 16-bit colour
    _tft.writecommand(0xB4); _tft.writedata(0x01);   // column inversion: 1-dot
    _tft.writecommand(0xB6);                           // display function control
        _tft.writedata(0x80); _tft.writedata(0x02); _tft.writedata(0x3B);
    _tft.writecommand(0xE8);                           // output ctrl adjust
        _tft.writedata(0x40); _tft.writedata(0x8A); _tft.writedata(0x00); _tft.writedata(0x00);
        _tft.writedata(0x29); _tft.writedata(0x19); _tft.writedata(0xA5); _tft.writedata(0x33);
    _tft.writecommand(0xC1); _tft.writedata(0x06);   // power control 2
    _tft.writecommand(0xC2); _tft.writedata(0xA7);   // power control 3
    _tft.writecommand(0xC5); _tft.writedata(0x18);   // VCOM = 0.9 V
    _tft.writecommand(0xE0);                           // gamma positive
        _tft.writedata(0xF0); _tft.writedata(0x09); _tft.writedata(0x0b); _tft.writedata(0x06);
        _tft.writedata(0x04); _tft.writedata(0x15); _tft.writedata(0x2F); _tft.writedata(0x54);
        _tft.writedata(0x42); _tft.writedata(0x3C); _tft.writedata(0x17); _tft.writedata(0x14);
        _tft.writedata(0x18); _tft.writedata(0x1B);
    _tft.writecommand(0xE1);                           // gamma negative
        _tft.writedata(0xE0); _tft.writedata(0x09); _tft.writedata(0x0B); _tft.writedata(0x06);
        _tft.writedata(0x04); _tft.writedata(0x03); _tft.writedata(0x2B); _tft.writedata(0x43);
        _tft.writedata(0x42); _tft.writedata(0x3B); _tft.writedata(0x16); _tft.writedata(0x14);
        _tft.writedata(0x17); _tft.writedata(0x1B);
    _tft.writecommand(0xF0); _tft.writedata(0x3C);   // disable extension cmd set 2 part I
    _tft.writecommand(0xF0); _tft.writedata(0x69);   // disable extension cmd set 2 part II

    _tft.writecommand(0x29);   // DISPON
    _tft.setRotation(1);       // restore MADCTL for our rotation
    _tft.fillScreen(TFT_BLACK);
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
