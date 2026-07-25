#include "SessionManager.h"
#include <EEPROM.h>
#include <string.h>
#include <math.h>

extern HWCDC USBSerial;

// --- EEPROM layout (see MenuSystem.h: EEPROM_SIZE == 256) ---
// Menu bindings live at 0..49, tire profiles at 50..111, and the menu "settings
// written" magic at EEPROM_SIZE-1 (255). The session summary blob gets its own region
// well clear of those. EEPROM.put writes the struct verbatim so signed/zero fields
// round-trip without the menu path's "byte can't store 0" caveat.
static const int     SESSION_EEPROM_ADDR = 128;
static const int     SESSION_MAGIC_ADDR  = 250;
static const uint8_t SESSION_MAGIC       = 0x53;

// Auto-seal backstop: seal after this much continuous IMU stillness.
static const unsigned long AUTO_SEAL_STILL_MS = 15000UL;
// A corner must be over the window this long before the overheat flag is raised, so a
// single smoothed blip doesn't stamp the summary.
static const unsigned long OVERHEAT_FLAG_MS   = 1000UL;

// Cap helper (Arduino's min() is a macro, so avoid the std::min template form here).
static inline unsigned long capUL(unsigned long v, unsigned long cap) {
    return (v > cap) ? cap : v;
}

SessionManager::SessionManager()
: running(false), winMin(0), winIdeal(0), winMax(0), unit('F'),
  elapsedMs(0), stillMs(0)
{
    memset(&lastSummary, 0, sizeof(lastSummary));
    lastSummary.valid = 0;
    lastSummary.unit  = 'F';
    resetAccumulators();
}

void SessionManager::resetAccumulators() {
    for (int i = 0; i < 4; i++) {
        seen[i] = false;
        peak[i] = 0.0f;
        steadySum[i] = 0.0; steadyCnt[i] = 0;
        allSum[i] = 0.0;    allCnt[i] = 0;
        inWindowMs[i] = 0;  overheatMs[i] = 0;
        firstReachedMs[i] = -1;
    }
    elapsedMs = 0;
    stillMs = 0;
}

void SessionManager::begin() {
    if (EEPROM.read(SESSION_MAGIC_ADDR) == SESSION_MAGIC) {
        EEPROM.get(SESSION_EEPROM_ADDR, lastSummary);
        lastSummary.unit = (lastSummary.unit == 'C') ? 'C' : 'F';
        if (lastSummary.valid != 1) lastSummary.valid = 0;
        USBSerial.println("Session summary loaded from EEPROM");
    } else {
        memset(&lastSummary, 0, sizeof(lastSummary));
        lastSummary.valid = 0;
        lastSummary.unit  = 'F';
        USBSerial.println("No saved session summary");
    }
}

void SessionManager::start(char u, float windowMin, float windowIdeal, float windowMax) {
    if (running) return;
    resetAccumulators();
    unit     = (u == 'C') ? 'C' : 'F';
    winMin   = windowMin;
    winIdeal = windowIdeal;
    winMax   = windowMax;
    running  = true;
    USBSerial.println("Session started");
}

void SessionManager::accumulate(long dtMillis, const float temps[4], const bool valid[4]) {
    if (!running || dtMillis <= 0) return;
    elapsedMs += (unsigned long)dtMillis;
    for (int i = 0; i < 4; i++) {
        if (!valid[i]) continue;
        float t = temps[i];
        if (!seen[i]) { seen[i] = true; peak[i] = t; }
        else if (t > peak[i]) peak[i] = t;

        allSum[i] += t; allCnt[i]++;

        if (t >= winMin) {
            if (firstReachedMs[i] < 0) firstReachedMs[i] = (long)elapsedMs;
            steadySum[i] += t; steadyCnt[i]++;
        }
        if (t >= winMin && t <= winMax) inWindowMs[i] += (unsigned long)dtMillis;
        if (t > winMax)                 overheatMs[i] += (unsigned long)dtMillis;
    }
}

void SessionManager::computeSummary() {
    SessionSummary& s = lastSummary;
    memset(&s, 0, sizeof(s));
    s.unit = unit;
    s.durationSec = (uint16_t)capUL(elapsedMs / 1000UL, 0xFFFEUL);

    // Per-corner rollup.
    float avgF[4];
    bool  avgValid[4];
    for (int i = 0; i < 4; i++) {
        s.cornerValid[i] = seen[i] ? 1 : 0;
        if (!seen[i]) { avgValid[i] = false; continue; }

        s.peak[i] = (int16_t)lroundf(peak[i]);

        float a;
        if (steadyCnt[i] > 0)      a = (float)(steadySum[i] / (double)steadyCnt[i]);
        else if (allCnt[i] > 0)    a = (float)(allSum[i] / (double)allCnt[i]);
        else                       a = peak[i];
        s.avg[i] = (int16_t)lroundf(a);
        avgF[i] = a; avgValid[i] = true;

        if (elapsedMs > 0) {
            unsigned long pct = (inWindowMs[i] * 100UL) / elapsedMs;
            s.inWindowPct[i] = (uint8_t)capUL(pct, 100UL);
        }
        s.overheat[i]    = (overheatMs[i] >= OVERHEAT_FLAG_MS) ? 1 : 0;
        s.overheatSec[i] = (uint16_t)capUL(overheatMs[i] / 1000UL, 0xFFFEUL);
    }

    // Warm-up: the moment ALL seen corners had first reached the window. If a seen
    // corner never warmed, warm-up is undefined (0xFFFF).
    long warm = 0;
    bool warmDefined = true;
    bool anySeen = false;
    for (int i = 0; i < 4; i++) {
        if (!seen[i]) continue;
        anySeen = true;
        if (firstReachedMs[i] < 0) { warmDefined = false; break; }
        if (firstReachedMs[i] > warm) warm = firstReachedMs[i];
    }
    s.warmupSec = (anySeen && warmDefined)
                    ? (uint16_t)capUL((unsigned long)warm / 1000UL, 0xFFFEUL)
                    : 0xFFFF;

    // Session-level balance from the per-corner steady averages (front=FL,FR;
    // rear=RL,RR; left=FL,RL; right=FR,RR). Average only the readable corners.
    auto pairAvg = [&](int a, int b, float& out) -> bool {
        float sum = 0.0f; int n = 0;
        if (avgValid[a]) { sum += avgF[a]; n++; }
        if (avgValid[b]) { sum += avgF[b]; n++; }
        if (n == 0) return false;
        out = sum / (float)n; return true;
    };
    float fa, ra, la, rra;
    bool fok = pairAvg(0, 1, fa), rok = pairAvg(2, 3, ra);
    bool lok = pairAvg(0, 2, la), rrok = pairAvg(1, 3, rra);
    s.frontRearDelta = (fok && rok) ? (int16_t)lroundf(fa - ra) : 0;
    s.leftRightDelta = (lok && rrok) ? (int16_t)lroundf(la - rra) : 0;

    s.valid = 1;
}

void SessionManager::persist() {
    EEPROM.put(SESSION_EEPROM_ADDR, lastSummary);
    EEPROM.write(SESSION_MAGIC_ADDR, SESSION_MAGIC);
    EEPROM.commit();
    USBSerial.println("Session summary saved to EEPROM");
}

void SessionManager::end() {
    if (!running) return;
    running = false;   // seal: no further frames are accumulated
    computeSummary();
    persist();
    USBSerial.println("Session sealed");
}

bool SessionManager::pollAutoSeal(long dtMillis, bool enabled, bool still) {
    if (!running || !enabled) { stillMs = 0; return false; }
    if (still) {
        if (dtMillis > 0) stillMs += (unsigned long)dtMillis;
        if (stillMs >= AUTO_SEAL_STILL_MS) {
            USBSerial.println("Auto-seal: sustained stillness");
            end();
            return true;
        }
    } else {
        stillMs = 0;
    }
    return false;
}

// --- Full-screen summary renderer --------------------------------------------------
// Screen is 280 wide x 240 tall (rotation 3), same frame the menu/balance views use.

static void drawWindowBar(Adafruit_ST7789& d, int16_t x, int16_t y,
                          int16_t w, int16_t h, uint8_t pct) {
    d.drawRect(x, y, w, h, ST77XX_WHITE);
    int16_t fillW = (int16_t)(((int32_t)(w - 2) * (int32_t)pct) / 100);
    if (fillW > 0) d.fillRect(x + 1, y + 1, fillW, h - 2, ST77XX_GREEN);
}

static void drawCorner(Adafruit_ST7789& d, int16_t x0, int16_t y0,
                       const char* label, const SessionSummary& s, int i) {
    d.setFont(nullptr);
    d.setTextSize(1);
    d.setTextColor(ST77XX_YELLOW);
    d.setCursor(x0 + 4, y0 + 2);
    d.print(label);

    if (!s.cornerValid[i]) {
        d.setTextColor(ST77XX_WHITE);
        d.setCursor(x0 + 4, y0 + 20);
        d.setTextSize(2);
        d.print(F("--"));
        return;
    }

    // Peak (large).
    d.setTextSize(3);
    d.setTextColor(s.overheat[i] ? ST77XX_RED : ST77XX_WHITE);
    d.setCursor(x0 + 4, y0 + 14);
    d.print((int)s.peak[i]);
    d.setTextSize(1);
    d.print((char)s.unit);

    // Average (small).
    d.setTextSize(1);
    d.setTextColor(ST77XX_WHITE);
    d.setCursor(x0 + 4, y0 + 44);
    d.print(F("avg "));
    d.print((int)s.avg[i]);

    // Time-in-window bar + percent.
    drawWindowBar(d, x0 + 4, y0 + 56, 110, 10, s.inWindowPct[i]);
    d.setCursor(x0 + 4 + 114, y0 + 57);
    d.print((int)s.inWindowPct[i]);
    d.print('%');

    // Overheat flag.
    if (s.overheat[i]) {
        d.setTextColor(ST77XX_RED);
        d.setCursor(x0 + 4, y0 + 70);
        d.print(F("HOT "));
        d.print((int)s.overheatSec[i]);
        d.print('s');
    }
}

void SessionManager::renderSummary(Adafruit_ST7789& d, const SessionSummary& s, int page) {
    d.fillScreen(ST77XX_BLACK);
    d.setFont(nullptr);

    if (!s.valid) {
        d.setTextSize(2);
        d.setTextColor(ST77XX_WHITE);
        d.setCursor(10, 100);
        d.print(F("No summary yet"));
        return;
    }

    if (page <= 0) {
        // Page 1: 2x2 car map, FL FR / RL RR.
        drawCorner(d, 0,   18, "FL", s, 0);
        drawCorner(d, 142, 18, "FR", s, 1);
        drawCorner(d, 0,   128, "RL", s, 2);
        drawCorner(d, 142, 128, "RR", s, 3);
        // Divider lines.
        d.drawFastVLine(140, 16, 214, 0x7BEF);
        d.drawFastHLine(0, 124, 280, 0x7BEF);

        d.setTextSize(1);
        d.setTextColor(0x7BEF);
        d.setCursor(4, 232);
        d.print(F("Peaks 1/2  up/dn:page  R:exit"));
    } else {
        // Page 2: session-level.
        d.setTextSize(2);
        d.setTextColor(ST77XX_WHITE);
        d.setCursor(10, 6);
        d.print(F("Session 2/2"));

        char u = (s.unit == 'C') ? 'C' : 'F';

        // F/R balance.
        d.setTextColor(ST77XX_WHITE);
        d.setCursor(10, 44);
        d.print(F("F/R d="));
        if (s.frontRearDelta > 0) d.print('+');
        d.print((int)s.frontRearDelta);
        d.print(u);
        d.setTextColor(ST77XX_YELLOW);
        d.setCursor(10, 66);
        d.print(s.frontRearDelta > 0 ? F("Understeer")
              : s.frontRearDelta < 0 ? F("Oversteer") : F("Neutral"));

        // L/R balance.
        d.setTextColor(ST77XX_WHITE);
        d.setCursor(10, 100);
        d.print(F("L/R d="));
        if (s.leftRightDelta > 0) d.print('+');
        d.print((int)s.leftRightDelta);
        d.print(u);
        d.setTextColor(ST77XX_YELLOW);
        d.setCursor(10, 122);
        d.print(s.leftRightDelta > 0 ? F("Left hot")
              : s.leftRightDelta < 0 ? F("Right hot") : F("Even"));

        // Warm-up + length.
        d.setTextColor(ST77XX_WHITE);
        d.setCursor(10, 156);
        d.print(F("Warmup: "));
        if (s.warmupSec == 0xFFFF) d.print(F("--"));
        else { d.print((int)s.warmupSec); d.print('s'); }

        d.setCursor(10, 178);
        d.print(F("Length: "));
        d.print((int)(s.durationSec / 60));
        d.print(F("m "));
        d.print((int)(s.durationSec % 60));
        d.print('s');

        d.setTextSize(1);
        d.setTextColor(0x7BEF);
        d.setCursor(4, 232);
        d.print(F("Session 2/2  up/dn:page  R:exit"));
    }
}
