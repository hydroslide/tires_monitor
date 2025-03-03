#include "MenuSystem.h"

extern HWCDC USBSerial;

// Forward declarations of recursive helpers
static void saveMenuToEEPROMHelper(const MenuItem* menu, uint8_t count);
static void loadMenuFromEEPROMHelper(const MenuItem* menu, uint8_t count);

MenuSystem::MenuSystem(const MenuItem* rootMenu, uint8_t menuCount)
: stackDepth(0)
{


  // Initialize the root menu context
  menuStack[0].menu = rootMenu;
  menuStack[0].count = menuCount;
  menuStack[0].selectedIndex = 0;
}

// Helper to get the current menu context
MenuSystem::MenuContext* MenuSystem::currentContext() {
  return &menuStack[stackDepth];
}

// Navigate to next item in the current menu
void MenuSystem::nextItem() {
  MenuContext* ctx = currentContext();
  if (ctx->count == 0) return;
  ctx->selectedIndex = (ctx->selectedIndex + 1) % ctx->count;
}

// Navigate to previous item in the current menu
void MenuSystem::previousItem() {
  MenuContext* ctx = currentContext();
  if (ctx->count == 0) return;
  if (ctx->selectedIndex == 0)
    ctx->selectedIndex = ctx->count - 1;
  else
    ctx->selectedIndex--;
}

// Select the currently highlighted item
void MenuSystem::selectItem() {
  MenuContext* ctx = currentContext();
  if (ctx->count == 0) return;

  const MenuItem* item = &ctx->menu[ctx->selectedIndex];
  if (!item) return;

  switch (item->itemType) {
    case MENU_ACTION:
      if (item->action) {
        // Execute the callback
        item->action();
      }
      break;

    case MENU_SUBMENU:
      // Go deeper if there's room in the stack
      if (item->submenu && item->submenuCount > 0 && stackDepth < (MAX_MENU_DEPTH - 1)) {
        stackDepth++;
        menuStack[stackDepth].menu = item->submenu;
        menuStack[stackDepth].count = item->submenuCount;
        menuStack[stackDepth].selectedIndex = 0;
      }
      break;

    case MENU_VALUE:
      // Optionally enter editing mode here if desired
      // e.g., numeric editing or toggles
      break;
  }
}

// Go back to the parent menu
bool MenuSystem::goBack() {
  if (stackDepth > 0) {
    stackDepth--;
    return true;
  }
  return false;
}

// Increase the current value (if the current item is a value)
void MenuSystem::increaseValue() {
  MenuContext* ctx = currentContext();
  if (ctx->count == 0) return;

  const MenuItem* item = &ctx->menu[ctx->selectedIndex];
  if (!item || item->itemType != MENU_VALUE || !item->binding) return;

  MenuValueBinding* b = item->binding;
  switch (b->valueType) {
    case VALUE_BYTE: {
      uint8_t* val = (uint8_t*)b->valuePtr;
      if (*val < b->maxByte) {
        (*val)++;
      }
      break;
    }
    case VALUE_BOOL: {
      bool* val = (bool*)b->valuePtr;
      *val = true;
      break;
    }
    case VALUE_STRING:
      // Implement string increment logic if needed
      break;
    case VALUE_ENUM: {
      // For an enum, increment the index
      uint8_t* enumVal = (uint8_t*)b->valuePtr;
      if (*enumVal + 1 < b->enumCount) {
        (*enumVal)++;
      }
      break;
    }
  }
}

// Decrease the current value (if the current item is a value)
void MenuSystem::decreaseValue() {
  MenuContext* ctx = currentContext();
  if (ctx->count == 0) return;

  const MenuItem* item = &ctx->menu[ctx->selectedIndex];
  if (!item || item->itemType != MENU_VALUE || !item->binding) return;

  MenuValueBinding* b = item->binding;
  switch (b->valueType) {
    case VALUE_BYTE: {
      uint8_t* val = (uint8_t*)b->valuePtr;
      if (*val > b->minByte) {
        (*val)--;
      }
      break;
    }
    case VALUE_BOOL: {
      bool* val = (bool*)b->valuePtr;
      *val = false;
      break;
    }
    case VALUE_STRING:
      // Implement string decrement logic if needed
      break;
    case VALUE_ENUM: {
      uint8_t* enumVal = (uint8_t*)b->valuePtr;
      if (*enumVal > 0) {
        (*enumVal)--;
      }
      break;
    }
  }
}

// Render the current menu (stub)
void MenuSystem::render() {
  // This could be a debug print or left empty if using an external renderer
  // e.g., Serial.println("MenuSystem::render() called");
}

// Return pointer to the current selected menu item
const MenuItem* MenuSystem::getCurrentItem() const {
  const MenuContext& ctx = menuStack[stackDepth];
  if (ctx.count == 0) return nullptr;
  return &ctx.menu[ctx.selectedIndex];
}

// Reset to the root menu
void MenuSystem::reset() {
  stackDepth = 0;
  menuStack[0].selectedIndex = 0;
}


static uint8_t itemsSaved=0;
// Recursive helper for saving an entire menu
static void saveMenuToEEPROMHelper(const MenuItem* menu, uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    const MenuItem* item = &menu[i];
    if (!item) continue;

    // If this is a value, save it
    if (item->itemType == MENU_VALUE && item->binding) {
      MenuValueBinding* b = item->binding;
      uint16_t addr = b->eepromAddress;
      switch (b->valueType) {
        case VALUE_BYTE:
          EEPROM.write(addr, *(uint8_t*)b->valuePtr);
            USBSerial.print(item->title);
            USBSerial.print(": ");
            USBSerial.print((String)*(uint8_t*)b->valuePtr);
            USBSerial.print(" written to: ");
            USBSerial.println((String)addr);
          itemsSaved++;
          break;
        case VALUE_BOOL:
          EEPROM.write(addr, *(bool*)b->valuePtr);
          break;
        case VALUE_STRING:
          EEPROM.put(addr, (char*)b->valuePtr);
          break;
        case VALUE_ENUM:
          EEPROM.write(addr, *(uint8_t*)b->valuePtr);
          break;
      }
    } else if (item->itemType == MENU_SUBMENU && item->submenu && item->submenuCount > 0) {
      // Recurse into the submenu
      saveMenuToEEPROMHelper(item->submenu, item->submenuCount);
    }
  }
}

static uint8_t itemsLoaded=0;

// Recursive helper for loading entire menu
static void loadMenuFromEEPROMHelper(const MenuItem* menu, uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    const MenuItem* item = &menu[i];
    if (!item) continue;

    // If it's a value item, load it
    if (item->itemType == MENU_VALUE && item->binding) {
      MenuValueBinding* b = item->binding;
      uint16_t addr = b->eepromAddress;
      switch (b->valueType) {
        case VALUE_BYTE:
          if (EEPROM.read(addr) > 0 && EEPROM.read(addr) < 255){            
            *(uint8_t*)b->valuePtr = EEPROM.read(addr);
            
            USBSerial.print(item->title);
            USBSerial.print(": ");
            USBSerial.print((String)*(uint8_t*)b->valuePtr);
            USBSerial.print(" read from: ");
            USBSerial.println((String)addr);
            itemsLoaded++;
          }
          break;
        case VALUE_BOOL:
          *(bool*)b->valuePtr = EEPROM.read(addr);
          break;
        case VALUE_STRING: {
          // For strings, we can't pass an rvalue. So we store it in a pointer:
          char* strPtr = (char*)b->valuePtr;
          EEPROM.get(addr, strPtr);
          break;
        }
        case VALUE_ENUM:
          *(uint8_t*)b->valuePtr = EEPROM.read(addr);
          break;
      }      
    }
    // If it's a submenu, recurse
    else if (item->itemType == MENU_SUBMENU && item->submenu && item->submenuCount > 0) {
      loadMenuFromEEPROMHelper(item->submenu, item->submenuCount);
    }
  }
}


// Save all configurable values from the entire menu tree to EEPROM
void MenuSystem::saveToEEPROM() {
  itemsSaved=0;
  // The root is always at stackDepth=0
  const MenuContext& rootCtx = menuStack[0];
  saveMenuToEEPROMHelper(rootCtx.menu, rootCtx.count);
  EEPROM.commit();
  USBSerial.print(String(itemsSaved));
  USBSerial.println(" items saved into EEPROM");
  loadFromEEPROM();
}

// Load all configurable values from the entire menu tree to EEPROM
void MenuSystem::loadFromEEPROM() {
  const MenuContext& rootCtx = menuStack[0];
  itemsLoaded=0;
  loadMenuFromEEPROMHelper(rootCtx.menu, rootCtx.count);
  USBSerial.print(String(itemsLoaded));
  USBSerial.println(" items loaded from EEPROM");
}

// Return pointer to the current menu array
const MenuItem* MenuSystem::getCurrentMenuItems() const {
  return menuStack[stackDepth].menu;
}

// Return number of items in the current menu
uint8_t MenuSystem::getCurrentMenuCount() const {
  return menuStack[stackDepth].count;
}

// Return index of the currently selected item
uint8_t MenuSystem::getCurrentSelectedIndex() const {
  return menuStack[stackDepth].selectedIndex;
}
