#ifndef TIRE_MENU_H
#define TIRE_MENU_H

//
#include <Arduino.h>
#include "MenuSystem.h"

/**
 * Exposes the global Tire MenuSystem instance,
 * giving read/write access to all menu items.
 */
MenuSystem &getTireMenuSystem();

#endif // TIRE_MENU_H
