#include "TireBalance.h"
#include "TempReader.h"

// Corner order matches the rest of the firmware: 0=FL, 1=FR, 2=RL, 3=RR.

// Whole-tire average for one corner: the mean of the three section bands for a
// camera tire, or the single working temp for a point sensor. Uses the working
// temps (tireSectionTemps / tireTemps), which already carry the calculated
// offset + smoothing when calculated mode is on.
static float cornerAvg(TempReader* r, int t) {
    if (r->tireSensorIsCamera[t]) {
        return (r->tireSectionTemps[t][0] +
                r->tireSectionTemps[t][1] +
                r->tireSectionTemps[t][2]) / 3.0f;
    }
    return r->tireTemps[t];
}

// Average the readable corners of a pairing. A corner reading <= 0 is treated as
// unread/invalid and dropped (same convention as the inflation read). Returns false
// when neither corner of the pair is readable.
static bool pairAvg(const float c[4], const bool v[4], int a, int b, float& out) {
    float sum = 0.0f;
    int n = 0;
    if (v[a]) { sum += c[a]; n++; }
    if (v[b]) { sum += c[b]; n++; }
    if (n == 0) return false;
    out = sum / (float)n;
    return true;
}

BalanceResult TireBalance::compute(TempReader* reader) {
    BalanceResult out;
    out.valid = false;
    out.frontAvg = out.rearAvg = out.leftAvg = out.rightAvg = 0.0f;
    out.frontRearDelta = out.leftRightDelta = 0.0f;
    out.frBias = "--";
    out.lrBias = "--";
    if (!reader) return out;

    float c[4];
    bool  v[4];
    for (int t = 0; t < 4; t++) {
        c[t] = cornerAvg(reader, t);
        v[t] = (c[t] > 0.0f);
    }

    float fa, ra, la, rra;
    bool fok  = pairAvg(c, v, 0, 1, fa);   // front = FL, FR
    bool rok  = pairAvg(c, v, 2, 3, ra);   // rear  = RL, RR
    bool lok  = pairAvg(c, v, 0, 2, la);   // left  = FL, RL
    bool rrok = pairAvg(c, v, 1, 3, rra);  // right = FR, RR
    if (!(fok && rok && lok && rrok)) return out;

    out.valid    = true;
    out.frontAvg = fa;
    out.rearAvg  = ra;
    out.leftAvg  = la;
    out.rightAvg = rra;
    out.frontRearDelta = fa - ra;
    out.leftRightDelta = la - rra;

    // Fronts hotter => understeer; rears hotter => oversteer. No threshold -- small
    // deltas are reported as-is (the pair carries the absolute levels).
    out.frBias = (out.frontRearDelta > 0.0f) ? "Understeer"
               : (out.frontRearDelta < 0.0f) ? "Oversteer"
               : "Neutral";
    // Left/right lopsidedness is expected at a directional track; the number lets the
    // crew judge whether it is the *expected* bias or a one-side problem.
    out.lrBias = (out.leftRightDelta > 0.0f) ? "Left hot"
               : (out.leftRightDelta < 0.0f) ? "Right hot"
               : "Even";
    return out;
}
