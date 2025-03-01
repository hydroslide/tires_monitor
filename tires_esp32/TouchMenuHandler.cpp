#include "TouchMenuHandler.h"
//#include "CST816Touch.h"

extern HWCDC USBSerial;

TouchMenuHandler::TouchMenuHandler(MenuSystem& menuSystem, MenuRenderer& renderer, CST816Touch& touch)
: menu(menuSystem), render(renderer), touchSensor(touch)
{
}

bool TouchMenuHandler::isMenuActive(){
    return menuActive;
}

void TouchMenuHandler::loop(int timeDelta) {
    // Poll the touch sensor
    touchSensor.control();

    TouchScreenEventCache* pTouchCache = TouchScreenEventCache::getInstance();
	
    lastTouchDelta+=timeDelta;

    TouchScreenController::gesture_t gesture = TouchScreenController::gesture_t::GESTURE_NONE;
	if (pTouchCache->hadGesture()) {
		int x = 0;
        int y = 0; 
        pTouchCache->getLastGesture(gesture, x, y);	//this 'consumes' the gesture from the event cache
        if (lastTouchDelta>= touchDelay){
            lastTouchDelta=0;        
          
            USBSerial.print("Gesture: ");
            USBSerial.print(TouchScreenController::gestureIdToString(gesture));
            if ((gesture == TouchScreenController::gesture_t::GESTURE_DOUBLE_CLICK) || (gesture == TouchScreenController::gesture_t::GESTURE_LONG_PRESS)) {
                USBSerial.print(", at position (");
                USBSerial.print(x);
                USBSerial.print(", ");
                USBSerial.print(y);
                USBSerial.print(")");
            }
            USBSerial.println("");
        }
        else
            gesture = TouchScreenController::gesture_t::GESTURE_NONE;
	}



    if (gesture == TouchScreenController::gesture_t::GESTURE_NONE) {
        // No new gesture
        return;
    }

    // Handle the gesture
    handleGesture(gesture);

    if (menuActive){
        // After handling, re-render
        render.render();
    }
}

void TouchMenuHandler::handleGesture(TouchScreenController::gesture_t gesture) {
    MenuRenderState &rState = render.getState();

   
    if (!menuActive){
        if (gesture == TouchScreenController::gesture_t::GESTURE_LEFT)
            menuActive = true;
        return;
    }

    switch (gesture) {
    case TouchScreenController::gesture_t::GESTURE_UP:
        if (!rState.dropdownOpen && !rState.numericEditing) {
            // Normal menu navigation up
            menu.previousItem();
        } else if (rState.dropdownOpen) {
            // Move up in dropdown
            render.dropdownUp();
        } else if (rState.numericEditing) {
            // Increase numeric value
            menu.increaseValue();
        }
        break;

    case TouchScreenController::gesture_t::GESTURE_DOWN:
        if (!rState.dropdownOpen && !rState.numericEditing) {
            // Normal menu navigation down
            menu.nextItem();
        } else if (rState.dropdownOpen) {
            // Move down in dropdown
            render.dropdownDown();
        } else if (rState.numericEditing) {
            // Decrease numeric value
            menu.decreaseValue();
        }
        break;

    case TouchScreenController::gesture_t::GESTURE_RIGHT:
        // If numeric editing or a dropdown is open, exit that mode
        if (rState.numericEditing) {
            rState.numericEditing = false;
        } else if (rState.dropdownOpen) {
            // Close the dropdown
            render.closeDropdown();
        } else {
            // Go back one level in the menu
           if (!menu.goBack()){
                menuActive=false;
                return;
           }
        }
        break;

    case TouchScreenController::gesture_t::GESTURE_LEFT:
    case TouchScreenController::gesture_t::GESTURE_TOUCH_BUTTON:
        if (rState.dropdownOpen) {
            // If user single-taps while dropdown is open, select
            render.selectDropdownValue();
        } else if (rState.numericEditing) {
            // Tapping while numeric editing - no special action by default
        } else {
            // Not editing anything, interpret tap as 'select' for the current menu item
            const MenuItem* currentItem = menu.getCurrentItem();
            if (!currentItem) break;

            if (currentItem->itemType == MENU_VALUE && currentItem->binding) {
                switch (currentItem->binding->valueType) {
                case VALUE_BOOL: {
                    // Toggle on single tap
                    bool* val = (bool*)currentItem->binding->valuePtr;
                    *val = !(*val);
                    break;
                }
                case VALUE_ENUM: {
                    // Open dropdown for an enum
                    uint8_t currentEnumVal = *(uint8_t*)currentItem->binding->valuePtr;
                    render.openDropdown(currentItem, currentEnumVal);
                    break;
                }
                case VALUE_BYTE: {
                    // Enter numeric editing mode
                    rState.numericEditing = true;
                    break;
                }
                case VALUE_STRING:
                    // Could open a keyboard or do nothing
                    break;
                }
            }
            else if (currentItem->itemType == MENU_ACTION) {
                // Execute the action
                menu.selectItem();
            }
            else if (currentItem->itemType == MENU_SUBMENU) {
                // Go deeper
                menu.selectItem();
            }
        }
        break;

    case TouchScreenController::gesture_t::GESTURE_DOUBLE_CLICK:
    case TouchScreenController::gesture_t::GESTURE_LONG_PRESS:
        // Not specified, ignoring or for advanced features
        break;

    case TouchScreenController::gesture_t::GESTURE_NONE:
    default:
        // No action
        break;
    }
}
