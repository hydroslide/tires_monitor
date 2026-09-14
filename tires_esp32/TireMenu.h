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

/**
 * Snap the active tire profile to the current mode's default profile (#14).
 *
 * Modes no longer own a temp window -- each one names a default profile (Street Settings /
 * Track Settings -> "Default Profile") and the active profile supplies the window. Call
 * this once at boot (after TireProfiles::begin(), before the display is built) and again
 * whenever Current Mode changes. A manual pick in Tire Profiles overrides it until the
 * next mode change or reboot, which is why the active slot is never persisted.
 */
void applyModeDefaultProfile();
// Keep the Tire Profiles edit buffer in step with the "Profile" selector (#18). Call from
// loop(); replaces the old manual "Load" menu item.
void serviceProfileEditSync();

/**
 * Poll (and clear) the "Set Offsets" request raised from Tire Profiles -> Offsets (#23).
 *
 * A MENU_ACTION callback runs deep inside the gesture handler and has no way to close the
 * menu or hand the screen to another mode, so it raises this flag and the sketch does both
 * from loop(). Returns true exactly once per pick.
 */
bool consumeOffsetSetupRequest();

/**
 * Poll (and clear) the "Set Camera Degrees" request raised from Display (#31).
 *
 * Same contract as consumeOffsetSetupRequest() above, for the same reason: the action
 * callback cannot close the menu or hand off the screen from where it runs. Returns true
 * exactly once per pick.
 */
bool consumeLensSetupRequest();

/** The lens-correction settings, read by the sketch to configure TempReader (#31). */
uint8_t getLensFovDegrees();
void    setLensFovDegrees(uint8_t v);
bool    getLensCorrectEnabled();
bool    getLensFitToView();

#endif // TIRE_MENU_H
