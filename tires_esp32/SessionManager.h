#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include <Arduino.h>
#include <Adafruit_ST7789.h>

// Session lifecycle + end-of-session summary (design 5.3, story 01 / issue #2).
//
// A session has clean boundaries so the slow cruise back and the paddock idle do not
// dilute per-corner peaks / averages / time-in-window. The driver starts and ends a
// session with a swipe (Track mode only); ending SEALS accumulation at that instant.
// The sealed recap is a per-corner 2x2 car map plus a session-level page (F/R + L/R
// balance and warm-up time), persisted to EEPROM (overwriting the previous), recalled
// via the menu's View Summary, and emitted over NBP.
//
// All temperatures here are the calculated working values (calculated-everywhere): the
// caller feeds the whole-tire working temp per corner, which already carries the
// offset + smoothing when calculated mode is on.

// Corner order matches the rest of the firmware: 0=FL, 1=FR, 2=RL, 3=RR.
struct SessionSummary {
    uint8_t  valid;            // 1 once a sealed summary has been written
    char     unit;             // 'F' or 'C' captured at session start
    uint8_t  cornerValid[4];   // 1 when the corner produced any reading
    int16_t  peak[4];          // peak working temp, rounded, per corner
    int16_t  avg[4];           // average steady (in/above-window) working temp
    uint8_t  inWindowPct[4];   // fraction of the session spent in [min,max], 0..100
    uint8_t  overheat[4];      // 1 when the corner exceeded windowMax long enough
    uint16_t overheatSec[4];   // roughly how long the corner was over windowMax
    uint16_t warmupSec;        // time until ALL seen corners first reached the window
                               // (0xFFFF => a seen corner never warmed / no data)
    uint16_t durationSec;      // sealed session length
    int16_t  frontRearDelta;   // avg(FL,FR) - avg(RL,RR), rounded (>0 => understeer)
    int16_t  leftRightDelta;   // avg(FL,RL) - avg(FR,RR), rounded (>0 => lefts hotter)
    // Per-corner inflation verdict on-time (story 06, made per-tire by #21). Fraction of
    // CAPTURED (straight-line) time that corner held a latched verdict, and which verdict
    // dominated. Surfaced on the summary per corner, only where on-time is >= 50%.
    // Was a single global pair; a rolled-up number could say something was wrong but
    // never which tire, which is the one thing you need to act on it.
    uint8_t  inflationOnPct[4];   // 0..100, over captured time
    int8_t   inflationVerdict[4]; // +1 over, -1 under, 0 none/insufficient
};

class SessionManager {
public:
    SessionManager();

    // Load the last persisted summary (or mark none present). Call after EEPROM.begin().
    void begin();

    bool isRunning()  const { return running; }
    bool hasSummary() const { return lastSummary.valid != 0; }
    const SessionSummary& summary() const { return lastSummary; }

    // Start a fresh session. Captures the window thresholds (in the working unit) and
    // the temperature unit for the summary. No-op if already running.
    void start(char unit, float windowMin, float windowIdeal, float windowMax);

    // Seal the session: freeze accumulation, compute the summary, persist it, and mark
    // it available. No-op if not running.
    void end();

    // Accumulate one reading tick (call on the read cadence, Track mode only, while
    // running). temps[] are the per-corner whole-tire working temps; valid[] flags the
    // readable corners. Frames after end() are never seen (running is false).
    void accumulate(long dtMillis, const float temps[4], const bool valid[4]);

    // Accumulate inflation-indicator on-time (story 06). Call on the read cadence, Track
    // mode only, while running. Only CAPTURED (straight-line) frames count toward the
    // denominator; alert is the latched verdict this tick (+1 over, -1 under, 0 none).
    void accumulateInflation(long dtMillis, bool capturing, const int8_t alert[4]);

    // Best-effort auto-seal backstop (design 5.3): when enabled and the car has been
    // essentially still for a sustained dwell (IMU near-rest -- there is no speed
    // channel), seal the session so a forgotten swipe still cuts the cruise/idle.
    // still == "IMU reports near-zero motion this tick". Returns true if it just sealed.
    bool pollAutoSeal(long dtMillis, bool enabled, bool still);

    // Full-screen summary renderer, shared by the auto-post-seal path (sketch) and the
    // menu View Summary action. page 0 = 2x2 car map, page 1 = session-level.
    static const int PAGE_COUNT = 2;
    static void renderSummary(Adafruit_ST7789& d, const SessionSummary& s, int page);

private:
    bool  running;

    // Captured window (working unit) for the active session.
    float winMin, winIdeal, winMax;
    char  unit;

    // Accumulators (per corner), reset at start().
    bool          seen[4];
    float         peak[4];
    double        steadySum[4];   // sum of in/above-window samples
    uint32_t      steadyCnt[4];
    double        allSum[4];       // fallback average when a corner never warmed
    uint32_t      allCnt[4];
    unsigned long inWindowMs[4];
    unsigned long overheatMs[4];
    long          firstReachedMs[4]; // elapsed ms when the corner first reached window
    unsigned long elapsedMs;

    // Inflation on-time (story 06): captured (straight-line) time and time-in-over/under.
    unsigned long capturedMs;      // whole-car: capture is not a per-corner state
    unsigned long inflOverMs[4];
    unsigned long inflUnderMs[4];

    unsigned long stillMs;         // running still-time for the auto-seal backstop

    SessionSummary lastSummary;

    void resetAccumulators();
    void computeSummary();
    void persist();
};

#endif // SESSION_MANAGER_H
