#ifndef MENU_RENDERER_H
#define MENU_RENDERER_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "MenuSystem.h"


struct MenuRenderState {
    bool dropdownOpen;
    bool numericEditing;
    uint8_t dropdownIndex;
    const MenuItem* dropdownItem;
    // Small-screen name entry (story 04): swipe up/down cycles the current letter,
    // swipe left (the menu's confirm direction) locks it and advances to the next slot
    // to the right; finishing past the last slot commits.
    bool nameEditing;
    // Balance summary screen (story 05): a full-screen Track-mode readout of the
    // front/rear and left/right thermal balance. Any gesture dismisses it.
    bool balanceViewing;
    // Session summary screen (story 01): a full-screen, multi-page recall of the last
    // sealed session. up/down pages, any other gesture dismisses it.
    bool summaryViewing;
    uint8_t summaryPage;
};

class MenuRenderer {
public:
    MenuRenderer(MenuSystem &menuSystem, Adafruit_ST7789 &tft);

    // Renders the current menu
    void render();

    // Access to the render state
    MenuRenderState &getState() { return state; }

    // Dropdown controls
    void openDropdown(const MenuItem* item, uint8_t startingIndex = 0);
    void closeDropdown();
    void dropdownUp();
    void dropdownDown();
    void selectDropdownValue();

    // 1) New status message setter. Renders in the reserved bottom strip (see
    // drawStatusStrip) for `ms` milliseconds, then auto-clears.
    void setStatusMessage(const char* msg, uint16_t ms = 2000);

    // Name-entry mode (story 04). beginNameEdit copies the target string into the
    // working buffer; nameCycle steps the current letter; nameAdvance locks it and
    // moves on, returning true (and committing back into target) once past the last
    // slot; nameCancel discards.
    void beginNameEdit(char* target, uint8_t maxLen);
    void nameCycle(int dir);
    bool nameAdvance();
    void nameCancel();
    // Move the cursor to the previous slot (the menu's back direction, swipe right);
    // retreating past the first slot cancels the whole edit (discards the working
    // buffer, restores the prior name).
    void nameRetreat();

    // Balance summary screen (story 05). showBalance takes over the whole screen with
    // the front/rear + left/right readout; exitBalance dismisses it.
    void showBalance();
    void exitBalance();

    // Session summary screen (story 01). showSummary takes over the whole screen with
    // the recalled last summary; summaryPageStep pages through it; exitSummary dismisses.
    void showSummary();
    void exitSummary();
    void summaryPageStep(int dir);

private:
    MenuSystem &menu;
    Adafruit_ST7789 &display;
    MenuRenderState state;

    byte textSize=2;

    // First visible item index — the menu list scrolls so the selection stays visible.
    int16_t menuScrollOffset = 0;

    unsigned long messageSetMillis;
    uint16_t messageDurationMs;

    // 2) We'll store the status message here
    static const uint8_t STATUS_MSG_LEN = 30;
    char statusMessage[STATUS_MSG_LEN+1];

    // Helper methods
    void drawMenuItem(const MenuItem &item, uint8_t index, bool selected);
    // Draws the reserved status strip at the bottom (separator + any active message).
    void drawStatusStrip();
    // Number of menu rows that fit above the reserved status strip.
    uint8_t menuVisibleRows() const;
    void drawBooleanValue(bool val, int16_t x, int16_t y);
    void drawEnumValue(uint8_t enumIndex, const MenuValueBinding *binding, int16_t x, int16_t y);
    void renderDropdown(const MenuItem &item);
    void renderNameEditor();
    void renderBalanceView();
    void renderSummaryView();

    // Name-editor working state.
    static const uint8_t NAME_EDIT_MAX = 12;
    char*   nameTarget = nullptr;
    uint8_t nameMax = 0;
    uint8_t namePos = 0;
    char    nameBuf[NAME_EDIT_MAX + 1];

    // Layout constants
    static const int16_t SCREEN_WIDTH = 280;
    static const int16_t SCREEN_HEIGHT = 240;
    static const int16_t STATUS_STRIP_HEIGHT = 22; // reserved bottom strip for status messages
    static const int16_t MENU_ITEM_HEIGHT = 20;
    static const int16_t MENU_LEFT_MARGIN = 10;
    static const int16_t MENU_TOP_MARGIN = 30;

    static const int16_t DROPDOWN_ITEM_HEIGHT = 18;
    static const int16_t DROPDOWN_WIDTH = 120;
    static const int16_t DROPDOWN_BG_COLOR = 0x7BEF;
    static const int16_t DROPDOWN_HIGHLIGHT_COLOR = 0xFFE0;
};

#endif
