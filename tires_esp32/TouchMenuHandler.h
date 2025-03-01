#ifndef TOUCH_MENU_HANDLER_H
#define TOUCH_MENU_HANDLER_H

#include <Arduino.h>
//#include "CST816Touch.h"
#include "CST816_TouchLib.h"
#include "MenuSystem.h"
#include "MenuRenderer.h"


using namespace MDO;

class TouchMenuHandler {
public:
    // Updated: pass a reference to CST816Touch instead of the old type
    TouchMenuHandler(MenuSystem& menuSystem, MenuRenderer& renderer, CST816Touch& touch);

    // Call this in your main loop
    void loop(int timeDelta);

    bool isMenuActive();

private:
    MenuSystem& menu;
    MenuRenderer& render;
    CST816Touch& touchSensor; // from the official library

    bool menuActive;
    int touchDelay=200;
    int lastTouchDelta=0;
    // Handle an incoming gesture
    void handleGesture(TouchScreenController::gesture_t gesture);
};

#endif
