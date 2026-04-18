#include "Display.h"
#include "../config.h"
#include "splash.h"

// ---------------------------------------------------------------------------
// Colour palette
// ---------------------------------------------------------------------------
static constexpr uint16_t COL_BG       = TFT_BLACK;
static constexpr uint16_t COL_HEADER   = 0x1082;   // near-black grey
static constexpr uint16_t COL_TEXT     = TFT_WHITE;
static constexpr uint16_t COL_ACCENT   = TFT_YELLOW;
static constexpr uint16_t COL_FWD      = TFT_GREEN;
static constexpr uint16_t COL_REV      = TFT_RED;
static constexpr uint16_t COL_BAR_BG   = 0x2104;   // dark grey
static constexpr uint16_t COL_DIVIDER  = 0x4208;
static constexpr uint16_t COL_SELECTED = 0x1E3F;

// Per-throttle bar colours
static constexpr uint16_t COL_BAR[4] = {
    TFT_CYAN, TFT_GREEN, TFT_YELLOW, TFT_MAGENTA
};

// ---------------------------------------------------------------------------
// Throttle column layout  (320 wide × 240 tall, 4 × 80px columns)
// ---------------------------------------------------------------------------
static constexpr int W       = 320;
static constexpr int H       = 240;
static constexpr int COL_W   = 80;    // 320 / 4
static constexpr int HDR_H   = 25;    // y 0-24
static constexpr int BAR_Y   = 26;    // y 26-173  (1px separator at y=25)
static constexpr int BAR_H   = 148;
static constexpr int SPD_Y   = 174;   // y 174-207  (FreeSansBold18 ~26px)
static constexpr int SPD_H   = 34;
static constexpr int DIR_Y   = 208;   // y 208-239  (FreeSansBold12 ~18px)
static constexpr int BAR_MRG = 8;     // horizontal bar margin inside column

// Roster layout
static constexpr int ROSTER_HDR_H  = 36;
static constexpr int ROSTER_ROW_H  = 32;
static constexpr int STATUS_H      = 24;

// ---------------------------------------------------------------------------

void Display::begin() {
#if TFT_BL_PIN >= 0
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);
#endif
    _tft.init();
    _tft.setRotation(1);   // landscape 320×240
    _tft.pushImage(0, 0, 320, 240, splash_pixels);
    delay(2000);
}

// ---------------------------------------------------------------------------
// Roster helpers (shared with drawRosterScreen)
// ---------------------------------------------------------------------------
void Display::drawHeader(const char *title, uint16_t bg, uint16_t fg) {
    _tft.fillRect(0, 0, W, ROSTER_HDR_H, bg);
    _tft.setTextColor(fg, bg);
    _tft.setTextSize(2);
    _tft.setCursor(8, 10);
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
// Throttle screen — full redraw of all 4 columns
// ---------------------------------------------------------------------------
void Display::drawThrottleScreen(const LocoState *locos, int count, bool connected) {
    _tft.fillScreen(COL_BG);
    for (int i = 1; i < 4; i++)
        _tft.drawFastVLine(i * COL_W, 0, H, COL_DIVIDER);
    for (int i = 0; i < count && i < 4; i++)
        drawThrottleColumn(i, locos[i], connected);
}

// ---------------------------------------------------------------------------
// Single column — called on encoder change to avoid full-screen redraws
// ---------------------------------------------------------------------------
void Display::drawThrottleColumn(int col, const LocoState &loco, bool connected) {
    int x = col * COL_W;

    // --- Header ---
    _tft.fillRect(x, 0, COL_W - 1, HDR_H, COL_HEADER);
    _tft.fillCircle(x + COL_W - 11, HDR_H / 2, 4, connected ? TFT_GREEN : TFT_RED);
    _tft.setFreeFont(&FreeSans9pt7b);
    _tft.setTextColor(COL_TEXT);
    _tft.setTextDatum(MC_DATUM);
    char addr[8];
    snprintf(addr, sizeof(addr), "#%d", loco.address);
    _tft.drawString(addr, x + 30, HDR_H / 2);

    // --- Separator ---
    _tft.drawFastHLine(x, 25, COL_W - 1, COL_DIVIDER);

    // --- Vertical speed bar — draw filled + unfilled in one pass, no clear step ---
    int bx     = x + BAR_MRG;
    int bw     = COL_W - 1 - 2 * BAR_MRG;
    int innerH = BAR_H - 2;
    int fillH  = map(loco.speed, 0, 126, 0, innerH);
    int emptyH = innerH - fillH;

    _tft.drawRect(bx, BAR_Y, bw, BAR_H, COL_TEXT);
    if (emptyH > 0)
        _tft.fillRect(bx + 1, BAR_Y + 1,          bw - 2, emptyH, COL_BAR_BG);
    if (fillH > 0)
        _tft.fillRect(bx + 1, BAR_Y + 1 + emptyH, bw - 2, fillH,  COL_BAR[col & 3]);

    // --- Speed value ---
    _tft.fillRect(x, SPD_Y, COL_W - 1, SPD_H, COL_BG);
    _tft.setFreeFont(&FreeSansBold18pt7b);
    _tft.setTextColor(COL_TEXT);
    _tft.setTextDatum(MC_DATUM);
    char spd[4];
    snprintf(spd, sizeof(spd), "%d", loco.speed);
    _tft.drawString(spd, x + (COL_W - 1) / 2, SPD_Y + SPD_H / 2);

    // --- Direction ---
    _tft.fillRect(x, DIR_Y, COL_W - 1, H - DIR_Y, COL_BG);
    _tft.setFreeFont(&FreeSansBold12pt7b);
    _tft.setTextColor(loco.forward ? COL_FWD : COL_REV);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString(loco.forward ? "FWD" : "REV", x + (COL_W - 1) / 2, DIR_Y + (H - DIR_Y) / 2);

    // Reset to built-in font for any subsequent default-font drawing
    _tft.setTextFont(1);
    _tft.setTextDatum(TL_DATUM);
}

// ---------------------------------------------------------------------------
// Speed-only update — bar + number, leaves header and direction untouched
// ---------------------------------------------------------------------------
void Display::drawThrottleSpeed(int col, const LocoState &loco) {
    int x      = col * COL_W;
    int bx     = x + BAR_MRG;
    int bw     = COL_W - 1 - 2 * BAR_MRG;
    int innerH = BAR_H - 2;
    int fillH  = map(loco.speed, 0, 126, 0, innerH);
    int emptyH = innerH - fillH;

    _tft.drawRect(bx, BAR_Y, bw, BAR_H, COL_TEXT);
    if (emptyH > 0)
        _tft.fillRect(bx + 1, BAR_Y + 1,          bw - 2, emptyH, COL_BAR_BG);
    if (fillH > 0)
        _tft.fillRect(bx + 1, BAR_Y + 1 + emptyH, bw - 2, fillH,  COL_BAR[col & 3]);

    _tft.fillRect(x, SPD_Y, COL_W - 1, SPD_H, COL_BG);
    _tft.setFreeFont(&FreeSansBold18pt7b);
    _tft.setTextColor(COL_TEXT);
    _tft.setTextDatum(MC_DATUM);
    char spd[4];
    snprintf(spd, sizeof(spd), "%d", loco.speed);
    _tft.drawString(spd, x + (COL_W - 1) / 2, SPD_Y + SPD_H / 2);

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
        int idx       = scrollOffset + i;
        bool hilight  = (entries[idx].address == DEFAULT_LOCO_ADDRESS);
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
