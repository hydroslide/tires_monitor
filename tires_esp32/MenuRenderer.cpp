
#include "MenuRenderer.h"
#include <string.h> // for strncpy
#include "TempReader.h"
#include "TireBalance.h"
#include "SessionManager.h"

// The active reader and temperature-scale selection live in the sketch; the balance
// summary reads the current working temps straight from them (story 05).
extern TempReader* tempReader;
extern uint8_t getTemperatureScaleValue();
// The session manager lives in the sketch; View Summary recalls its last summary.
extern SessionManager sessionManager;

MenuRenderer::MenuRenderer(MenuSystem &menuSystem, Adafruit_ST7789 &tft)
: menu(menuSystem), display(tft)
{
    state.dropdownOpen = false;
    state.numericEditing = false;
    state.dropdownIndex = 0;
    state.dropdownItem = nullptr;
    state.nameEditing = false;
    state.balanceViewing = false;
    state.summaryViewing = false;
    state.summaryPage = 0;
    nameBuf[0] = '\0';

    // Initialize status message as empty
    statusMessage[0] = '\0';
}

void MenuRenderer::render() {
    // Clear the screen
    display.fillScreen(ST77XX_BLACK);

    // Guardrail: never let GFX wrap text onto the next line. A value too wide for
    // its column would otherwise advance the cursor down and paint over the row
    // below it (issue #12); with wrap off, over-long text is clipped/truncated
    // instead. Also keeps getTextBounds measurements from inflating on overflow.
    display.setTextWrap(false);

    // Session summary takes over the whole screen while active (story 01).
    if (state.summaryViewing) {
        renderSummaryView();
        return;
    }

    // Balance summary takes over the whole screen while active (story 05).
    if (state.balanceViewing) {
        renderBalanceView();
        return;
    }

    // Name-entry mode takes over the whole screen while active.
    if (state.nameEditing) {
        renderNameEditor();
        return;
    }

    // Retrieve current menu array, item count, selected index
    const MenuItem* items = menu.getCurrentMenuItems();
    uint8_t itemCount = menu.getCurrentMenuCount();
    uint8_t selectedIndex = menu.getCurrentSelectedIndex();

    // Scroll the list so the selection stays visible above the reserved status strip.
    uint8_t visibleRows = menuVisibleRows();
    if (itemCount <= visibleRows) {
        menuScrollOffset = 0;
    } else {
        if (selectedIndex < menuScrollOffset)
            menuScrollOffset = selectedIndex;
        else if (selectedIndex >= menuScrollOffset + visibleRows)
            menuScrollOffset = selectedIndex - visibleRows + 1;
        int16_t maxOffset = (int16_t)itemCount - visibleRows;
        if (menuScrollOffset > maxOffset) menuScrollOffset = maxOffset;
        if (menuScrollOffset < 0) menuScrollOffset = 0;
    }

    // Render only the visible window; drawMenuItem positions by on-screen row.
    uint8_t firstItem = (uint8_t)menuScrollOffset;
    uint8_t lastItem  = firstItem + visibleRows;
    if (lastItem > itemCount) lastItem = itemCount;
    for (uint8_t i = firstItem; i < lastItem; i++) {
        drawMenuItem(items[i], (uint8_t)(i - firstItem), (i == selectedIndex));
    }

    // Subtle scrollbar on the right edge when the list overflows the window.
    if (itemCount > visibleRows) {
        int16_t trackX  = SCREEN_WIDTH - 4;
        int16_t trackTop = MENU_TOP_MARGIN;
        int16_t trackH  = visibleRows * MENU_ITEM_HEIGHT;
        int16_t thumbH  = trackH * visibleRows / itemCount;
        if (thumbH < 8) thumbH = 8;
        int16_t denom   = (int16_t)itemCount - visibleRows;
        int16_t thumbY  = trackTop + (denom > 0 ? (trackH - thumbH) * menuScrollOffset / denom : 0);
        display.fillRect(trackX, trackTop, 3, trackH, 0x2104); // dark track
        display.fillRect(trackX, thumbY, 3, thumbH, 0x7BEF);   // gray thumb
    }

    // If a dropdown is open, draw the overlay
    if (state.dropdownOpen && state.dropdownItem) {
        renderDropdown(*state.dropdownItem);
    }

    // Reserved bottom strip for transient status messages ("Saved", etc.).
    drawStatusStrip();
}

void MenuRenderer::setStatusMessage(const char* msg, uint16_t ms) {
    if (!msg) {
        statusMessage[0] = '\0';
        return;
    }
    strncpy(statusMessage, msg, STATUS_MSG_LEN);
    statusMessage[STATUS_MSG_LEN] = '\0';
    messageSetMillis = millis();
    messageDurationMs = ms;
}

uint8_t MenuRenderer::menuVisibleRows() const {
    int16_t listBottom = SCREEN_HEIGHT - STATUS_STRIP_HEIGHT;
    int16_t avail = listBottom - MENU_TOP_MARGIN;
    if (avail < MENU_ITEM_HEIGHT) return 1;
    return (uint8_t)(avail / MENU_ITEM_HEIGHT);
}

// Reserved strip along the bottom for transient status text. Always draws its
// separator so the layout is stable whether or not a message is showing.
void MenuRenderer::drawStatusStrip() {
    int16_t stripTop = SCREEN_HEIGHT - STATUS_STRIP_HEIGHT;
    display.drawFastHLine(0, stripTop, SCREEN_WIDTH, 0x7BEF);

    if (statusMessage[0] == '\0') return;
    if (millis() - messageSetMillis >= messageDurationMs) {
        statusMessage[0] = '\0'; // timed out -> clear
        return;
    }
    display.setFont(nullptr);
    display.setTextSize(2);
    display.setTextColor(ST77XX_WHITE);
    display.setCursor(6, stripTop + 4);
    display.print(statusMessage);
}



void MenuRenderer::openDropdown(const MenuItem* item, uint8_t startingIndex) {
    state.dropdownOpen = true;
    state.dropdownIndex = startingIndex;
    state.dropdownItem = item;
}

void MenuRenderer::closeDropdown() {
    state.dropdownOpen = false;
    state.dropdownItem = nullptr;
}

void MenuRenderer::dropdownUp() {
    // Move selection up (wrap around if needed)
    if (!state.dropdownOpen || !state.dropdownItem) return;
    MenuValueBinding* b = state.dropdownItem->binding;
    if (!b || b->valueType != VALUE_ENUM) return;

    if (state.dropdownIndex == 0)
        state.dropdownIndex = b->enumCount - 1;
    else
        state.dropdownIndex--;
}

void MenuRenderer::dropdownDown() {
    // Move selection down (wrap around if needed)
    if (!state.dropdownOpen || !state.dropdownItem) return;
    MenuValueBinding* b = state.dropdownItem->binding;
    if (!b || b->valueType != VALUE_ENUM) return;

    state.dropdownIndex = (state.dropdownIndex + 1) % b->enumCount;
}

void MenuRenderer::selectDropdownValue() {
    // Apply the selected enum index to the actual item->binding->valuePtr
    if (!state.dropdownOpen || !state.dropdownItem) return;
    MenuValueBinding* b = state.dropdownItem->binding;
    if (!b || b->valueType != VALUE_ENUM) return;

    // Assign the new value
    *(uint8_t*)(b->valuePtr) = state.dropdownIndex;

    // Then close the dropdown
    closeDropdown();
}

// Draw a single menu item
void MenuRenderer::drawMenuItem(const MenuItem &item, uint8_t index, bool selected) {
    int16_t x = MENU_LEFT_MARGIN;
    int16_t y = MENU_TOP_MARGIN + index * MENU_ITEM_HEIGHT;

    // Highlight if selected
    if (selected) {
        // Draw highlight background
        display.fillRect(0, y, SCREEN_WIDTH, MENU_ITEM_HEIGHT, ST77XX_YELLOW);
        if (state.numericEditing)
            display.setTextColor(ST77XX_RED);
        else
            display.setTextColor(ST77XX_BLACK);
    } else {
        display.setTextColor(ST77XX_WHITE);
    }

    display.setCursor(x, y);
    display.setFont(nullptr);     // <— back to the built-in 5×7 font 
    display.setTextSize(textSize);

    // Print the item title
    display.print(item.title);

    // If it's a value, show the value on the right
    if (item.itemType == MENU_VALUE && item.binding) {
        switch (item.binding->valueType) {
            case VALUE_BOOL: {
                bool currentVal = *(bool*)item.binding->valuePtr;
                drawBooleanValue(currentVal, SCREEN_WIDTH - 60, y);
                break;
            }
            case VALUE_BYTE: {
                uint8_t currentVal = *(uint8_t*)item.binding->valuePtr;
                display.setCursor(SCREEN_WIDTH - 60, y);
                display.print(currentVal);
                break;
            }
            case VALUE_SBYTE: {
                int8_t currentVal = *(int8_t*)item.binding->valuePtr;
                display.setCursor(SCREEN_WIDTH - 60, y);
                display.print((int)currentVal);
                break;
            }
            case VALUE_STRING: {
                char* strVal = (char*)item.binding->valuePtr;
                int16_t vx = SCREEN_WIDTH - 80;
                drawClipped(strVal, vx, y, SCREEN_WIDTH - vx - VALUE_RIGHT_PAD);
                break;
            }
            case VALUE_ENUM: {
                uint8_t enumIndex = *(uint8_t*)item.binding->valuePtr;
                drawEnumValue(enumIndex, item.binding, (SCREEN_WIDTH - 80), y);
                break;
            }
        }
    }
}

void MenuRenderer::drawBooleanValue(bool val, int16_t x, int16_t y) {
    // We'll represent booleans with [x] or [ ]
    display.setCursor(x, y);
    if (val) {
        display.print(F("[X]")); 
    } else {
        display.print(F("[ ]")); 
    }
}

void MenuRenderer::drawEnumValue(uint8_t enumIndex, const MenuValueBinding *binding,
                                 int16_t x, int16_t y)
{
    // We'll assume binding->enumLabels is valid
    if (enumIndex < binding->enumCount) {
        drawClipped(binding->enumLabels[enumIndex], x, y,
                    SCREEN_WIDTH - x - VALUE_RIGHT_PAD);
    }
}

// Draw `s` clipped to `maxW` pixels. Measures with getTextBounds; if the whole
// string fits it is printed as-is, otherwise the longest prefix that fits with a
// trailing ".." marker is drawn. The built-in 5x7 font has no real ellipsis
// glyph (it is CP437, not Unicode), so ".." is used as the truncation marker.
void MenuRenderer::drawClipped(const char *s, int16_t x, int16_t y, int16_t maxW) {
    if (!s || maxW <= 0) return;
    display.setCursor(x, y);

    int16_t bx, by;
    uint16_t bw, bh;
    display.getTextBounds(s, x, y, &bx, &by, &bw, &bh);
    if ((int16_t)bw <= maxW) {
        display.print(s);          // fits as-is, no marker needed
        return;
    }

    // Too wide: find the longest prefix such that "<prefix>.." fits in maxW.
    static const char MARKER[] = "..";
    char buf[40];
    int len = (int)strlen(s);
    for (int n = len; n >= 0; n--) {
        int m = 0;
        for (int i = 0; i < n && m < (int)sizeof(buf) - 3; i++) buf[m++] = s[i];
        buf[m++] = MARKER[0];
        buf[m++] = MARKER[1];
        buf[m]   = '\0';
        display.getTextBounds(buf, x, y, &bx, &by, &bw, &bh);
        if ((int16_t)bw <= maxW) {
            display.print(buf);
            return;
        }
    }
    // Even ".." alone won't fit — leave the column blank rather than overflow.
}

// Render the dropdown overlay for the specified item
void MenuRenderer::renderDropdown(const MenuItem &item) {
    // We only handle VALUE_ENUM in a dropdown
    if (!item.binding || item.binding->valueType != VALUE_ENUM) return;
    MenuValueBinding* b = item.binding;

    display.setFont(nullptr);
    display.setTextSize(textSize);

    const int16_t pad = 5;

    // Size the box to its widest label (capped to the screen) so options aren't
    // clipped in the common case; drawClipped below is the safety net if even the
    // capped width is too small. DROPDOWN_WIDTH is the minimum.
    int16_t ddWidth = DROPDOWN_WIDTH;
    if (b->enumLabels) {
        for (uint8_t i = 0; i < b->enumCount; i++) {
            if (!b->enumLabels[i]) continue;
            int16_t bx, by; uint16_t bw, bh;
            display.getTextBounds(b->enumLabels[i], 0, 0, &bx, &by, &bw, &bh);
            int16_t need = (int16_t)bw + 2 * pad;
            if (need > ddWidth) ddWidth = need;
        }
    }
    const int16_t maxBox = SCREEN_WIDTH - 8;
    if (ddWidth > maxBox) ddWidth = maxBox;

    // Center the (possibly widened) box on screen.
    int16_t ddX = (SCREEN_WIDTH - ddWidth) / 2;
    int16_t ddY = (SCREEN_HEIGHT - (b->enumCount * DROPDOWN_ITEM_HEIGHT)) / 2;

    // Draw a background rectangle
    int16_t ddHeight = b->enumCount * DROPDOWN_ITEM_HEIGHT;
    display.fillRect(ddX, ddY, ddWidth, ddHeight, DROPDOWN_BG_COLOR);

    // Draw each option
    for (uint8_t i = 0; i < b->enumCount; i++) {
        // If i == state.dropdownIndex, highlight it
        bool isSelected = (i == state.dropdownIndex);
        int16_t optionY = ddY + i * DROPDOWN_ITEM_HEIGHT;

        if (isSelected) {
            display.fillRect(ddX, optionY, ddWidth, DROPDOWN_ITEM_HEIGHT, DROPDOWN_HIGHLIGHT_COLOR);
            display.setTextColor(ST77XX_BLACK);
        } else {
            display.setTextColor(ST77XX_WHITE);
        }

        if (b->enumLabels && b->enumLabels[i]) {
            drawClipped(b->enumLabels[i], ddX + pad, optionY + 2, ddWidth - 2 * pad);
        }
    }
}

// --- Small-screen name entry (story 04) --------------------------------------------
// The cycle order for a letter: space, A-Z, then 0-9. Swipe up/down walks this ring.
static const char NAME_CHARSET[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

static int nameCharsetIndex(char c) {
    for (int i = 0; NAME_CHARSET[i] != '\0'; i++)
        if (NAME_CHARSET[i] == c) return i;
    return 0; // default to space for anything unexpected
}

void MenuRenderer::beginNameEdit(char* target, uint8_t maxLen) {
    if (!target) return;
    nameTarget = target;
    nameMax = (maxLen < NAME_EDIT_MAX) ? maxLen : NAME_EDIT_MAX;
    // Seed the working buffer from the current name, uppercased and space-padded.
    for (uint8_t i = 0; i < nameMax; i++) {
        char c = target[i];
        if (c == '\0') { for (uint8_t k = i; k < nameMax; k++) nameBuf[k] = ' '; break; }
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if (nameCharsetIndex(c) == 0 && c != ' ') c = ' ';
        nameBuf[i] = c;
    }
    nameBuf[nameMax] = '\0';
    namePos = 0;
    state.nameEditing = true;
}

void MenuRenderer::nameCycle(int dir) {
    if (!state.nameEditing || namePos >= nameMax) return;
    int len = (int)(sizeof(NAME_CHARSET) - 1); // excludes the null
    int idx = nameCharsetIndex(nameBuf[namePos]);
    idx = (idx + dir % len + len) % len;
    nameBuf[namePos] = NAME_CHARSET[idx];
}

bool MenuRenderer::nameAdvance() {
    if (!state.nameEditing) return true;
    namePos++;
    if (namePos >= nameMax) {
        // Commit: trim trailing spaces so "ECF   " stores as "ECF".
        int end = (int)nameMax;
        while (end > 0 && nameBuf[end - 1] == ' ') end--;
        if (nameTarget) {
            for (int i = 0; i < end; i++) nameTarget[i] = nameBuf[i];
            nameTarget[end] = '\0';
        }
        state.nameEditing = false;
        nameTarget = nullptr;
        return true;
    }
    return false;
}

void MenuRenderer::nameCancel() {
    state.nameEditing = false;
    nameTarget = nullptr;
}

void MenuRenderer::nameRetreat() {
    if (!state.nameEditing) return;
    if (namePos == 0) {
        // Backed out past the first slot -> cancel the whole edit (nameBuf is
        // discarded and nameTarget was never modified, so the prior name stands).
        nameCancel();
        return;
    }
    namePos--;
}

void MenuRenderer::renderNameEditor() {
    display.setFont(nullptr);
    display.setTextColor(ST77XX_WHITE);
    display.setTextSize(2);
    display.setCursor(10, 20);
    display.print(F("Name:"));

    // Draw each character slot; highlight the active one.
    int16_t x = 10;
    int16_t y = 70;
    display.setTextSize(3);
    for (uint8_t i = 0; i < nameMax; i++) {
        bool cur = (i == namePos);
        int16_t cw = 20;
        if (cur) {
            display.fillRect(x - 2, y - 4, cw, 32, ST77XX_YELLOW);
            display.setTextColor(ST77XX_BLACK);
        } else {
            display.setTextColor(ST77XX_WHITE);
        }
        display.setCursor(x, y);
        char c = nameBuf[i];
        display.print(c == ' ' ? '_' : c);
        x += cw;
    }

    display.setTextSize(1);
    display.setTextColor(ST77XX_WHITE);
    display.setCursor(10, SCREEN_HEIGHT - 30);
    display.print(F("Up/Dn: letter   Swipe L: next/save"));
    display.setCursor(10, SCREEN_HEIGHT - 16);
    display.print(F("Swipe R: back   R@start: cancel"));
}

// --- Balance summary screen (story 05) ----------------------------------------------
void MenuRenderer::showBalance() {
    state.balanceViewing = true;
}

void MenuRenderer::exitBalance() {
    state.balanceViewing = false;
}

// Draw one balance row: label, the pair "<a> / <b> <unit>", then the signed delta and
// the plain-language bias hint on the next line.
static void drawBalanceRow(Adafruit_ST7789& d, int16_t y, const char* label,
                           float a, float b, float delta, const char* hint, char unit) {
    d.setTextSize(2);
    d.setTextColor(ST77XX_WHITE);
    d.setCursor(10, y);
    d.print(label);
    d.print(' ');
    d.print((int)lroundf(a));
    d.print(F(" / "));
    d.print((int)lroundf(b));
    d.print(' ');
    d.print(unit);

    long dRounded = lroundf(delta);
    d.setTextSize(2);
    d.setCursor(24, y + 22);
    d.setTextColor(ST77XX_YELLOW);
    d.print(F("d="));
    if (dRounded > 0) d.print('+');
    d.print(dRounded);
    d.print(F("  "));
    d.print(hint);
}

void MenuRenderer::renderBalanceView() {
    display.setFont(nullptr);
    display.setTextColor(ST77XX_WHITE);
    display.setTextSize(2);
    display.setCursor(10, 6);
    display.print(F("Balance"));

    char unit = (getTemperatureScaleValue() == 0) ? 'F' : 'C';
    BalanceResult b = TireBalance::compute(tempReader);

    if (!b.valid) {
        display.setTextSize(2);
        display.setCursor(10, 70);
        display.print(F("No tire data"));
    } else {
        // Front/Rear: fronts hotter => understeer, rears hotter => oversteer.
        drawBalanceRow(display, 50, "F/R", b.frontAvg, b.rearAvg,
                       b.frontRearDelta, b.frBias, unit);
        // Left/Right: expected lopsided at a directional track; flags the unexpected.
        drawBalanceRow(display, 120, "L/R", b.leftAvg, b.rightAvg,
                       b.leftRightDelta, b.lrBias, unit);
    }

    display.setTextSize(1);
    display.setTextColor(ST77XX_WHITE);
    display.setCursor(10, SCREEN_HEIGHT - 20);
    display.print(F("Tap to return"));
}

// --- Session summary screen (story 01) ----------------------------------------------
void MenuRenderer::showSummary() {
    state.summaryViewing = true;
    state.summaryPage = 0;
}

void MenuRenderer::exitSummary() {
    state.summaryViewing = false;
}

void MenuRenderer::summaryPageStep(int dir) {
    int p = (int)state.summaryPage + dir;
    if (p < 0) p = 0;
    if (p >= SessionManager::PAGE_COUNT) p = SessionManager::PAGE_COUNT - 1;
    state.summaryPage = (uint8_t)p;
}

void MenuRenderer::renderSummaryView() {
    // renderSummary paints the whole frame (including its own fillScreen).
    SessionManager::renderSummary(display, sessionManager.summary(), state.summaryPage);
}
