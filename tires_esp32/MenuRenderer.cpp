
#include "MenuRenderer.h"
#include <string.h> // for strncpy

MenuRenderer::MenuRenderer(MenuSystem &menuSystem, Adafruit_ST7789 &tft)
: menu(menuSystem), display(tft)
{
    state.dropdownOpen = false;
    state.numericEditing = false;
    state.dropdownIndex = 0;
    state.dropdownItem = nullptr;

    // Initialize status message as empty
    statusMessage[0] = '\0';
}

void MenuRenderer::render() {
    // Clear the screen
    display.fillScreen(ST77XX_BLACK);

    // Retrieve current menu array, item count, selected index
    const MenuItem* items = menu.getCurrentMenuItems();
    uint8_t itemCount = menu.getCurrentMenuCount();
    uint8_t selectedIndex = menu.getCurrentSelectedIndex();

    // Render menu items
    for (uint8_t i = 0; i < itemCount; i++) {
        bool isSelected = (i == selectedIndex);
        drawMenuItem(items[i], i, isSelected);
    }

    // If a dropdown is open, draw the overlay
    if (state.dropdownOpen && state.dropdownItem) {
        renderDropdown(*state.dropdownItem);
    }



    // 3) Draw the status message (if any)
    if (statusMessage[0] != '\0') {
        if (millis() - messageSetMillis >= messageDurationMs) {
            statusMessage[0] = '\\0'; // clear the message
        } else {
            // Example: draw near bottom-left
            display.setCursor(10, SCREEN_HEIGHT - 20);
            display.setTextColor(ST77XX_WHITE);
            display.setTextSize(textSize);
            display.print(statusMessage);
        }
    }
}

void MenuRenderer::setStatusMessage(const char* msg) {
    if (!msg) {
        statusMessage[0] = '\\0';
        return;
    }
    strncpy(statusMessage, msg, STATUS_MSG_LEN);
    statusMessage[STATUS_MSG_LEN] = '\\0';
    messageSetMillis = millis();
    messageDurationMs = 2000;//durationMs;
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
            case VALUE_STRING: {
                char* strVal = (char*)item.binding->valuePtr;
                display.setCursor(SCREEN_WIDTH - 80, y);
                display.print(strVal);
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
        display.print(F("\"[x]\"")); 
    } else {
        display.print(F("\"[ ]\"")); 
    }
}

void MenuRenderer::drawEnumValue(uint8_t enumIndex, const MenuValueBinding *binding,
                                 int16_t x, int16_t y)
{
    // We'll assume binding->enumLabels is valid
    if (enumIndex < binding->enumCount) {
        display.setCursor(x, y);
        display.print(binding->enumLabels[enumIndex]);
    }
}

// Render the dropdown overlay for the specified item
void MenuRenderer::renderDropdown(const MenuItem &item) {
    // We only handle VALUE_ENUM in a dropdown
    if (!item.binding || item.binding->valueType != VALUE_ENUM) return;
    MenuValueBinding* b = item.binding;

    // We'll center the dropdown in the middle of the screen, or place it near the selected item
    // For simplicity, let's place it near the middle
    int16_t ddX = (SCREEN_WIDTH - DROPDOWN_WIDTH) / 2;
    int16_t ddY = (SCREEN_HEIGHT - (b->enumCount * DROPDOWN_ITEM_HEIGHT)) / 2;

    // Draw a background rectangle
    int16_t ddHeight = b->enumCount * DROPDOWN_ITEM_HEIGHT;
    display.fillRect(ddX, ddY, DROPDOWN_WIDTH, ddHeight, DROPDOWN_BG_COLOR);

    // Draw each option
    for (uint8_t i = 0; i < b->enumCount; i++) {
        // If i == state.dropdownIndex, highlight it
        bool isSelected = (i == state.dropdownIndex);
        int16_t optionY = ddY + i * DROPDOWN_ITEM_HEIGHT;

        if (isSelected) {
            display.fillRect(ddX, optionY, DROPDOWN_WIDTH, DROPDOWN_ITEM_HEIGHT, DROPDOWN_HIGHLIGHT_COLOR);
            display.setTextColor(ST77XX_BLACK);
        } else {
            display.setTextColor(ST77XX_WHITE);
        }

        display.setCursor(ddX + 5, optionY + 2); // Some padding
        display.setTextSize(textSize);
        if (b->enumLabels && i < b->enumCount) {
            display.print(b->enumLabels[i]);
        }
    }
}
