#ifndef MENU_RENDERER_H
#define MENU_RENDERER_H

//
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "MenuSystem.h"

// Rendering state to track dropdowns and submenus
struct MenuRenderState {
    bool dropdownOpen;           // Whether an enum dropdown is currently open
    bool numericEditing;         // NEW: Are we editing a numeric item?
    uint8_t dropdownIndex;       
    const MenuItem* dropdownItem; 
};

class MenuRenderer {
public:
    // Constructor: store references to the menu system and the display
    MenuRenderer(MenuSystem &menuSystem, Adafruit_ST7789 &tft);

    // Render the current menu (fully integrated now)
    void render();

    // Access to the render state
    MenuRenderState &getState() { return state; }

    // Methods for toggling and controlling the dropdown
    void openDropdown(const MenuItem* item, uint8_t startingIndex = 0);
    void closeDropdown();

    // Move selection in the dropdown
    void dropdownUp();
    void dropdownDown();

    // Apply the currently highlighted enum value
    void selectDropdownValue();

private:
    MenuSystem &menu;        
    Adafruit_ST7789 &display;
    MenuRenderState state;

    byte textSize=2;

    // Helper to draw a single item
    void drawMenuItem(const MenuItem &item, uint8_t index, bool selected);

    // Helper to draw boolean value
    void drawBooleanValue(bool val, int16_t x, int16_t y);

    // Helper to draw enum value
    void drawEnumValue(uint8_t enumIndex, const MenuValueBinding *binding, int16_t x, int16_t y);

    // Helper to draw the dropdown overlay
    void renderDropdown(const MenuItem &item);

    // Adjust these if your display is rotated
    static const int16_t SCREEN_WIDTH = 280;   // Was 240 before, now 280
    static const int16_t SCREEN_HEIGHT = 240;  // Was 280 before, now 240

    static const int16_t MENU_ITEM_HEIGHT = 20;
    static const int16_t MENU_LEFT_MARGIN = 10;
    static const int16_t MENU_TOP_MARGIN = 30;

    // Dropdown layout
    static const int16_t DROPDOWN_ITEM_HEIGHT = 18;
    static const int16_t DROPDOWN_WIDTH = 120; 
    static const int16_t DROPDOWN_BG_COLOR = 0x7BEF;        // or ST77XX_DARKERGREY
    static const int16_t DROPDOWN_HIGHLIGHT_COLOR = 0xFFE0; // or ST77XX_YELLOW
};

#endif // MENU_RENDERER_H
