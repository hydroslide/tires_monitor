#ifndef TIRE_BALANCE_H
#define TIRE_BALANCE_H

#include <Arduino.h>

// Front/Rear & Left/Right thermal balance (story 05, design section 5.2).
//
// Balance is the thermal fingerprint of handling: front-vs-rear reads the
// understeer/oversteer bias and left-vs-right catches the *unexpected* one-side
// problem. It compares each corner's WHOLE-TIRE AVERAGE working temperature -- the
// calculated (offset + EMA-smoothed) value of story 03, not the raw spiky surface --
// so the reading tracks the carcass the driver actually feels.
//
// All four corners are included (RR is not excluded; it works well enough for the
// balance readout). This is a summary/between-session readout, never a live bias
// arrow, and it is a Track-mode-only feature (currentMode == 1).

class TempReader;

struct BalanceResult {
    bool  valid;            // false when a pairing has no readable corner

    // Whole-tire calculated averages, in the active temperature unit.
    float frontAvg;         // avg of FL, FR
    float rearAvg;          // avg of RL, RR
    float leftAvg;          // avg of FL, RL
    float rightAvg;         // avg of FR, RR

    float frontRearDelta;   // frontAvg - rearAvg  (positive => fronts hotter)
    float leftRightDelta;   // leftAvg  - rightAvg (positive => lefts hotter)

    // Plain-language bias hints (see the design table).
    const char* frBias;     // "Understeer" / "Oversteer" / "Neutral"
    const char* lrBias;     // "Left hot" / "Right hot" / "Even"
};

namespace TireBalance {
    // Compute the balance from the reader's current working temps. When calculated
    // mode is active those working temps already carry the offset + smoothing, so the
    // averages are the calculated whole-tire values by construction; when it is off
    // they are the raw working temps (matching "calculated everywhere when enabled").
    BalanceResult compute(TempReader* reader);
}

#endif // TIRE_BALANCE_H
