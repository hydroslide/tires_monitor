#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include <stdint.h>
#include <EEPROM.h>

#ifdef __cplusplus
extern "C" {
#endif

// Menu item types
typedef enum {
  MENU_ACTION,   // Executes an action when selected
  MENU_SUBMENU,  // Contains submenu items
  MENU_VALUE     // Holds a configurable value
} MenuItemType;

// Value types for MENU_VALUE items
typedef enum {
  VALUE_BYTE,
  VALUE_BOOL,
  VALUE_STRING,
  VALUE_ENUM
} MenuValueType;

// Forward declaration for MenuItem structure
struct MenuItem;

// Function pointer type for action callbacks
typedef void (*ActionFunction)();

// Binding structure for configurable values
typedef struct {
  MenuValueType valueType; // Type of the value (byte, bool, string, or enum)
  void* valuePtr;          // Pointer to the variable to be updated
  void* defaultValuePtr;   // Pointer to the default value (same type as valuePtr)
  uint8_t minByte;         // Minimum value (if applicable)
  uint8_t maxByte;         // Maximum value (if applicable)
  uint16_t eepromAddress;  // EEPROM address for persistent storage
  const char** enumLabels; // Pointer to an array of enum labels (if VALUE_ENUM)
  uint8_t enumCount;       // Number of enum options (if VALUE_ENUM)
} MenuValueBinding;

// Structure defining a menu item
typedef struct MenuItem {
  const char* title;            // Text to display for the item
  MenuItemType itemType;        // Action, submenu, or value type
  ActionFunction action;        // Function to execute if an action (nullptr if not used)
  const struct MenuItem* submenu; // Pointer to an array of submenu items (if any)
  uint8_t submenuCount;         // Number of submenu items in the submenu array
  MenuValueBinding* binding;    // Pointer to the value binding (if a value type)
} MenuItem;

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

// Class to manage menu navigation and updates
class MenuSystem {
public:
  // Constructor takes the root menu array and its count
  MenuSystem(const MenuItem* rootMenu, uint8_t menuCount);

  // Navigation functions
  void nextItem();
  void previousItem();
  void selectItem();
  bool goBack();

  // Increase or decrease the value (if the current item is a value)
  void increaseValue();
  void decreaseValue();

  // Render the current menu (implementation of drawing is up to you)
  void render();

  // Get pointer to the current selected menu item
  const MenuItem* getCurrentItem() const;

  // Reset menu system to the root menu
  void reset();

  // Save all configurable values to EEPROM
  void saveToEEPROM();

  // Load all configurable values from EEPROM
  void loadFromEEPROM();

  // Additional getters for full integration:
  const MenuItem* getCurrentMenuItems() const; // Return pointer to current menu array
  uint8_t getCurrentMenuCount() const;         // Return number of items in current menu
  uint8_t getCurrentSelectedIndex() const;     // Return index of currently selected item

private:
  // Context used for menu navigation
  static const uint8_t MAX_MENU_DEPTH = 10;
  struct MenuContext {
    const MenuItem* menu;   // Pointer to the current menu array
    uint8_t count;          // Number of items in the current menu
    uint8_t selectedIndex;  // Currently selected index in this menu
  };

  MenuContext menuStack[MAX_MENU_DEPTH]; // Stack to support submenus
  uint8_t stackDepth;                    // Current depth in the menu stack

  // Helper to get the current context
  MenuContext* currentContext();
};

#endif // __cplusplus

#endif // MENU_SYSTEM_H
