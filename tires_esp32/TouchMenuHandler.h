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

    bool SwipedRight();
    bool SwipedUp();
    bool SwipedDown();
    // GESTURE_LEFT reaches the sketch only while the menu is suspended -- normally it is
    // swallowed to OPEN the menu (see handleGesture). Offset setup (#23) needs it as half of
    // its nudge pair, so it suspends the menu for the duration.
    bool SwipedLeft();

    // Suspend the "a left swipe opens the menu" behavior, so a full-screen mode can own all
    // four directions instead of three.
    void suspendMenu(bool suspend);

    // Force the menu open / closed. menuActive is otherwise reachable only through gestures,
    // and a menu ACTION that hands the screen to another mode (#23) has to be able to leave
    // the menu -- and to land back on the exact screen it left, which MenuSystem's own
    // navigation stack preserves for free.
    void openMenu();
    void closeMenu();

private:
    MenuSystem& menu;
    MenuRenderer& render;
    CST816Touch& touchSensor; // from the official library

    bool menuActive;
    bool menuSuspended = false;
    bool unhandledSwipeRight;
    bool unhandledSwipeLeft;
    bool unhandledSwipeUp;
    bool unhandledSwipeDown;
    int touchDelay=200;
    int lastTouchDelta=0;
    // Handle an incoming gesture
    void handleGesture(TouchScreenController::gesture_t gesture);
};

#endif
