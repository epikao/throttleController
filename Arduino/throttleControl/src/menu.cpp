#include <Arduino.h>
#include "button.h"
#include "storage.h"
#include "menu.h"
#include "globals.h"
#include "calc.h"
#include "ds3231.h"
#include "ui/ui.h"
#include "ui/screens.h"

uint8_t  menuState = 0, lastMenuState = 1;
uint32_t modeDebounce = 0;
volatile uint8_t stopwatchState = 0; // 0=idle, 1=running, 2=stopped

// ---- Stopwatch (Core 0) ------------------------------------------
// Akkumuliert Centisekunden aus dem realen millis()-Delta -> unabhaengig von der
// Loop-Rate (LVGL kann zeitweise >100ms blockieren, Gesamtzeit bleibt korrekt).
void stopwatch_update() {
    static uint32_t lastMs   = 0;
    static uint32_t remainMs = 0; // <10ms Rest, der noch keine Centisekunde ergibt
    uint32_t now = millis();
    if (stopwatchState == 1) {
        remainMs += now - lastMs;
        if (remainMs >= 10) {
            p.stopwatchTicks += remainMs / 10;
            remainMs %= 10;
        }
    } else {
        remainMs = 0;
    }
    lastMs = now;
}

// ---- Button state helpers ----------------------------------------

static inline void clearL() { btnL.released = 0; btnL.msCount = -1; }
static inline void clearR() { btnR.released = 0; btnR.msCount = -1; }
static void clearBoth() {
    clearL(); clearR();
    btnL.time1 = 0; btnR.time1 = 0;
}
static void defaultL() { btnL.released = 0; if (!btnL.pressed) btnL.msCount = -1; }
static void defaultR() { btnR.released = 0; if (!btnR.pressed) btnR.msCount = -1; }

// ---- Screen navigation helpers -----------------------------------

static void navLeft(uint8_t target, ScreensEnum screen) {
    menuState = target; loadScreen(screen); clearL();
}
static void navRight(uint8_t target, ScreensEnum screen) {
    menuState = target; loadScreen(screen); clearR();
}

// ---- Combined-button handlers ------------------------------------

static void handleBothShort(uint8_t base, uint8_t maxParam, bool doDebounce = true) {
    if (menuState != base) {
        if (++menuState > maxParam) menuState = base + 1;
    }
    clearBoth();
    if (doDebounce) modeDebounce = millis();
}

static void handleBothLong(uint8_t base) {
    menuState = (menuState != base) ? base : base + 1;
    modeDebounce = millis();
}

// ---- Parameter adjustment helpers --------------------------------
//
// Alle Helfer nehmen VOLATILE Referenzen: Die von Core 1 gelesenen Parameter in
// EBikeSettings sind volatile (siehe globals.h), und ein `volatile T` laesst sich nicht
// an ein `T&` binden ("discards qualifiers"). Umgekehrt ist es unproblematisch - ein
// `volatile T&` darf auch an ein NICHT-volatiles Objekt gebunden werden (Qualifier
// hinzufuegen ist erlaubt). Daher funktionieren diese Helfer fuer beide Sorten.

// float: wrap at bounds
static void adjL(volatile float &v, float s, float S, float lo, float hi) {
    if      (btnL.msCount < LONG_PRESS_TIME) { v -= s; if (v < lo) v = hi; clearL(); }
    else if (btnL.pressed)                    { v -= S; if (v < lo) v = hi; btnL.released = 0; }
    else                                      { clearL(); }
}
static void adjR(volatile float &v, float s, float S, float lo, float hi) {
    if      (btnR.msCount < LONG_PRESS_TIME) { v += s; if (v > hi) v = lo; clearR(); }
    else if (btnR.pressed)                   { v += S; if (v > hi) v = lo; btnR.released = 0; }
    else                                     { clearR(); }
}

// uint16_t with 12-bit bitmask wrap (torque params)
static void adjLm(volatile uint16_t &v, uint16_t s, uint16_t S, uint16_t mask) {
    if      (btnL.msCount < LONG_PRESS_TIME) { v = (v - s) & mask; clearL(); }
    else if (btnL.pressed)                    { v = (v - S) & mask; btnL.released = 0; }
    else                                      { clearL(); }
}
static void adjRm(volatile uint16_t &v, uint16_t s, uint16_t S, uint16_t mask) {
    if      (btnR.msCount < LONG_PRESS_TIME) { v = (v + s) & mask; clearR(); }
    else if (btnR.pressed)                   { v = (v + S) & mask; btnR.released = 0; }
    else                                     { clearR(); }
}

// uint8_t: wrap at bounds
static void adjL(volatile uint8_t &v, uint8_t s, uint8_t S, uint8_t /*lo*/, uint8_t hi) {
    if      (btnL.msCount < LONG_PRESS_TIME) { v = (v >= s) ? uint8_t(v - s) : hi; clearL(); }
    else if (btnL.pressed)                    { v = (v >= S) ? uint8_t(v - S) : hi; btnL.released = 0; }
    else                                      { clearL(); }
}
static void adjR(volatile uint8_t &v, uint8_t s, uint8_t S, uint8_t lo, uint8_t hi) {
    if      (btnR.msCount < LONG_PRESS_TIME) { v = uint8_t(v+s) <= hi ? uint8_t(v+s) : lo; clearR(); }
    else if (btnR.pressed)                   { v = uint8_t(v+S) <= hi ? uint8_t(v+S) : lo; btnR.released = 0; }
    else                                     { clearR(); }
}

// uint16_t: wrap at bounds
static void adjL(volatile uint16_t &v, uint16_t s, uint16_t S, uint16_t lo, uint16_t hi) {
    if      (btnL.msCount < LONG_PRESS_TIME) { v = v >= s + lo ? uint16_t(v-s) : hi; clearL(); }
    else if (btnL.pressed)                    { v = v >= S + lo ? uint16_t(v-S) : hi; btnL.released = 0; }
    else                                      { clearL(); }
}
static void adjR(volatile uint16_t &v, uint16_t s, uint16_t S, uint16_t lo, uint16_t hi) {
    if      (btnR.msCount < LONG_PRESS_TIME) { v = uint16_t(v+s) <= hi ? uint16_t(v+s) : lo; clearR(); }
    else if (btnR.pressed)                   { v = uint16_t(v+S) <= hi ? uint16_t(v+S) : lo; btnR.released = 0; }
    else                                     { clearR(); }
}

// Toggle uint8_t 0/1, no fast step
static void toggleL(volatile uint8_t &v) {
    if      (btnL.msCount < LONG_PRESS_TIME) { v ^= 1; clearL(); }
    else if (btnL.pressed)                    { btnL.released = 0; }
    else                                      { clearL(); }
}
static void toggleR(volatile uint8_t &v) {
    if      (btnR.msCount < LONG_PRESS_TIME) { v ^= 1; clearR(); }
    else if (btnR.pressed)                   { btnR.released = 0; }
    else                                     { clearR(); }
}

// Clamped (no wrap, no fast step)
static void clampL(volatile uint8_t &v, uint8_t lo) {
    if      (btnL.msCount < LONG_PRESS_TIME) { if (v > lo) v--; clearL(); }
    else if (btnL.pressed)                    { btnL.released = 0; }
    else                                      { clearL(); }
}
static void clampR(volatile uint8_t &v, uint8_t hi) {
    if      (btnR.msCount < LONG_PRESS_TIME) { if (v < hi) v++; clearR(); }
    else if (btnR.pressed)                   { btnR.released = 0; }
    else                                     { clearR(); }
}

// =================================================================
void menu_handler() {

    bool anyButton = btnL.pressed || btnR.pressed || btnL.released || btnR.released;
    static bool backlightJustWoke = false;

    if (anyButton) {
        if (!backlightJustWoke) {
            // Backlight-Zustand VOR dem Update prüfen
            bool backlightWasOff = (p.onTimeBacklight != 1000) &&
                                   ((millis() - lastButtonActivityMs) >=
                                    ((uint32_t)p.onTimeBacklight * 1000));
            if (backlightWasOff) backlightJustWoke = true;
        }
        lastButtonActivityMs = millis();
    }

    // Backlight gerade aufgewacht: Events schlucken bis Tasten losgelassen
    if (backlightJustWoke) {
        clearBoth();
        if (!btnL.pressed && !btnR.pressed) backlightJustWoke = false;
        return;
    }

    if ((millis() - modeDebounce) < (LONG_PRESS_TIME * 10)) {
        btnR.pressed = 0; btnL.pressed = 0;
        btnR.msCount = -1; btnL.msCount = -1;
        btnR.msCount2 = -1; btnL.msCount2 = -1;
        btnR.released = 0; btnL.released = 0;
        btnR.time1 = 0; btnL.time1 = 0;
    }

    bool bothShort = (labs(btnL.time1 - btnR.time1) < 400)
                  && btnL.time1 > 0 && btnR.time1 > 0
                  && (btnL.released || btnR.released)
                  && (btnR.msCount < LONG_PRESS_TIME)
                  && (btnL.msCount < LONG_PRESS_TIME);
    bool bothLong  = (btnR.msCount > LONG_PRESS_TIME) && (btnL.msCount > LONG_PRESS_TIME);
    bool leftRel   = btnL.released && btnL.time1 > 0 && !btnR.pressed;
    bool rightRel  = btnR.released && btnR.time1 > 0 && !btnL.pressed;

    if (menuState < 10) { // Screen 0: Main (param 1 = supportLevel)

        if (bothShort) {
            handleBothShort(0, 1);
        } else if (bothLong) {
            handleBothLong(0);
        } else if (leftRel) {
            if (menuState == 0 && btnL.msCount < LONG_PRESS_TIME) {
                navLeft(80, (ScreensEnum)9);
            } else if (menuState != 0) {
                storageDirty = true;
                switch (menuState) {
                    case 1:
                        if      (btnL.msCount < LONG_PRESS_TIME) { if (p.supportLevel >= 1) p.supportLevel--; clearL(); }
                        else if (btnL.pressed)                    { btnL.released = 0; }
                        else                                      { clearL(); }
                        break;
                    default: defaultL(); break;
                }
            }
        } else if (rightRel) {
            if (menuState == 0 && btnR.msCount < LONG_PRESS_TIME) {
                navRight(10, (ScreensEnum)2);
            } else if (menuState != 0) {
                storageDirty = true;
                switch (menuState) {
                    case 1:
                        if      (btnR.msCount < LONG_PRESS_TIME) { p.supportLevel++; if (p.supportLevel > 12) p.supportLevel = 12; clearR(); }
                        else if (btnR.pressed)                    { btnR.released = 0; }
                        else                                      { clearR(); }
                        break;
                    default: defaultR(); break;
                }
            }
        }

    } else if (menuState < 20) { // Screen 1: Torque (params 11-16)

        if (bothShort) {
            handleBothShort(10, 16);
        } else if (bothLong) {
            handleBothLong(10);
        } else if (leftRel) {
            Serial.println("btnL released");
            if (menuState == 10 && btnL.msCount < LONG_PRESS_TIME) {
                navLeft(0, (ScreensEnum)1);
            } else if (menuState != 10) {
                storageDirty = true;
                switch (menuState) {
                    case 11: adjLm(p.torqueZero,      1, 10, 0x0FFF); break;
                    case 12: adjLm(p.torqueMax,       1, 10, 0x0FFF); break;
                    case 13: adjLm(p.torqueDeadband,  1, 10, 0x0FFF); break;
                    case 14: adjL(p.torqueFilterRise, 0.001f, 0.01f, 0.0f, 1.0f); break;
                    case 15: adjL(p.torqueFilterFall, 0.001f, 0.01f, 0.0f, 1.0f); break;
                    case 16: adjL(p.torqueIdleMs, (uint16_t)1, (uint16_t)10, (uint16_t)0, (uint16_t)999); break;
                    default: defaultL(); break;
                }
            }
        } else if (rightRel) {
            Serial.println("btnR released");
            if (menuState == 10 && btnR.msCount < LONG_PRESS_TIME) {
                navRight(20, (ScreensEnum)3);
            } else if (menuState != 10) {
                storageDirty = true;
                switch (menuState) {
                    case 11: adjRm(p.torqueZero,      1, 10, 0x0FFF); break;
                    case 12: adjRm(p.torqueMax,       1, 10, 0x0FFF); break;
                    case 13: adjRm(p.torqueDeadband,  1, 10, 0x0FFF); break;
                    case 14: adjR(p.torqueFilterRise, 0.001f, 0.01f, 0.0f, 1.0f); break;
                    case 15: adjR(p.torqueFilterFall, 0.001f, 0.01f, 0.0f, 1.0f); break;
                    case 16: adjR(p.torqueIdleMs, (uint16_t)1, (uint16_t)10, (uint16_t)0, (uint16_t)999); break;
                    default: defaultR(); break;
                }
            }
        }

    } else if (menuState < 30) { // Screen 2: Cadence (params 21-26)

        if (bothShort) {
            handleBothShort(20, 27);
        } else if (bothLong) {
            handleBothLong(20);
        } else if (leftRel) {
            if (menuState == 20 && btnL.msCount < LONG_PRESS_TIME) {
                navLeft(10, (ScreensEnum)2);
            } else if (menuState != 20) {
                storageDirty = true;
                switch (menuState) {
                    case 21: clampL(p.pulsesPerRev, 1); break;
                    case 22:
                        if      (btnL.msCount < LONG_PRESS_TIME) { if (p.cadenceTimeoutMs >= 1)  p.cadenceTimeoutMs -= 1;  else p.cadenceTimeoutMs = 2000; clearL(); }
                        else if (btnL.pressed)                    { if (p.cadenceTimeoutMs >= 10) p.cadenceTimeoutMs -= 10; else p.cadenceTimeoutMs = 2000; btnL.released = 0; }
                        else                                      { clearL(); }
                        break;
                    case 23: adjL(p.cadenceMaxRpm,      0.1f,  1.0f,  1.0f, 150.0f); break;
                    case 24: adjL(p.cadenceFilterAlpha, 0.01f, 0.05f, 0.0f,   1.0f); break;
                    case 25: adjL(p.cadenceMaxLimit,    0.1f,  1.0f,  0.0f, 200.0f); break;
                    case 26: adjL(p.cadenceGatePulses, (uint8_t)1, (uint8_t)1, (uint8_t)1, (uint8_t)12); break;
                    case 27: adjL(p.cadenceGateMs, (uint16_t)10, (uint16_t)100, (uint16_t)100, (uint16_t)3000); break;
                    default: defaultL(); break;
                }
            }
        } else if (rightRel) {
            if (menuState == 20 && btnR.msCount < LONG_PRESS_TIME) {
                navRight(30, (ScreensEnum)4);
            } else if (menuState != 20) {
                storageDirty = true;
                switch (menuState) {
                    case 21: clampR(p.pulsesPerRev, 48); break;
                    case 22:
                        if      (btnR.msCount < LONG_PRESS_TIME) { if (p.cadenceTimeoutMs <= 2000-1)  p.cadenceTimeoutMs += 1;  else p.cadenceTimeoutMs = 0; clearR(); }
                        else if (btnR.pressed)                   { if (p.cadenceTimeoutMs <= 2000-10) p.cadenceTimeoutMs += 10; else p.cadenceTimeoutMs = 0; btnR.released = 0; }
                        else                                     { clearR(); }
                        break;
                    case 23: adjR(p.cadenceMaxRpm,      0.1f,  1.0f,  0.0f, 150.0f); break;
                    case 24: adjR(p.cadenceFilterAlpha, 0.01f, 0.05f, 0.0f,   1.0f); break;
                    case 25: adjR(p.cadenceMaxLimit,    0.1f,  1.0f,  0.0f, 200.0f); break;
                    case 26: adjR(p.cadenceGatePulses, (uint8_t)1, (uint8_t)1, (uint8_t)1, (uint8_t)12); break;
                    case 27: adjR(p.cadenceGateMs, (uint16_t)10, (uint16_t)100, (uint16_t)100, (uint16_t)3000); break;
                    default: defaultR(); break;
                }
            }
        }

    } else if (menuState < 40) { // Screen 3: Curve (params 31-37)

        if (bothShort) {
            handleBothShort(30, 38);
        } else if (bothLong) {
            handleBothLong(30);
        } else if (leftRel) {
            if (menuState == 30 && btnL.msCount < LONG_PRESS_TIME) {
                navLeft(20, (ScreensEnum)3);
            } else if (menuState != 30) {
                storageDirty = true;
                switch (menuState) {
                    case 31: adjL(p.curveY25,      0.01f,  0.05f, 0.0f, 1.0f); break;
                    case 32: adjL(p.curveY50,      0.01f,  0.05f, 0.0f, 1.0f); break;
                    case 33: adjL(p.curveY75,      0.01f,  0.05f, 0.0f, 1.0f); break;
                    case 34: adjL(p.curveOffset,   0.01f,  0.05f, 0.0f, 1.0f); break;
                    case 35: adjL(p.rampUpLow,     0.001f, 0.01f, 0.0f, 1.0f); break;
                    case 36: adjL(p.rampUpHigh,    0.001f, 0.01f, 0.0f, 1.0f); break;
                    case 37: adjL(p.rampThreshold, 0.01f,  0.05f, 0.0f, 1.0f); break;
                    case 38: adjL(p.rampDown,      0.001f, 0.01f, 0.0f, 1.0f); break;
                    default: defaultL(); break;
                }
            }
        } else if (rightRel) {
            if (menuState == 30 && btnR.msCount < LONG_PRESS_TIME) {
                navRight(40, (ScreensEnum)5);
            } else if (menuState != 30) {
                storageDirty = true;
                switch (menuState) {
                    case 31: adjR(p.curveY25,      0.01f,  0.05f, 0.0f, 1.0f); break;
                    case 32: adjR(p.curveY50,      0.01f,  0.05f, 0.0f, 1.0f); break;
                    case 33: adjR(p.curveY75,      0.01f,  0.05f, 0.0f, 1.0f); break;
                    case 34: adjR(p.curveOffset,   0.01f,  0.05f, 0.0f, 1.0f); break;
                    case 35: adjR(p.rampUpLow,     0.001f, 0.01f, 0.0f, 1.0f); break;
                    case 36: adjR(p.rampUpHigh,    0.001f, 0.01f, 0.0f, 1.0f); break;
                    case 37: adjR(p.rampThreshold, 0.01f,  0.05f, 0.0f, 1.0f); break;
                    case 38: adjR(p.rampDown,      0.001f, 0.01f, 0.0f, 1.0f); break;
                    default: defaultR(); break;
                }
            }
        }

    } else if (menuState < 50) { // Screen 4: Cruise/Battery (params 41-47)

        if (bothShort) {
            handleBothShort(40, 49);
        } else if (bothLong) {
            handleBothLong(40);
        } else if (leftRel) {
            if (menuState == 40 && btnL.msCount < LONG_PRESS_TIME) {
                navLeft(30, (ScreensEnum)4);
            } else if (menuState != 40) {
                storageDirty = true;
                switch (menuState) {
                    case 41: adjL(p.cruisePower,    (uint8_t)1, (uint8_t)5, (uint8_t)0, (uint8_t)100); break;
                    case 42: adjL(p.cruiseRampUp,   0.001f, 0.01f, 0.0f,  1.0f); break;
                    case 43: adjL(p.cruiseRampDown, 0.001f, 0.01f, 0.0f,  1.0f); break;
                    case 44: adjL(p.cruiseDeadband, 0.01f,  0.05f, 0.0f, 0.5f); break;
                    case 45: adjL(p.pasMode, (uint8_t)1, (uint8_t)1, (uint8_t)0, (uint8_t)3); break;
                    case 46: adjL(p.voltageMin,   0.1f,   1.0f,  0.0f, 99.9f); break;
                    case 47: adjL(p.voltageMax,   0.1f,   1.0f,  0.0f, 99.9f); break;
                    case 48: adjL(p.currentMax,   0.1f,   1.0f,  0.0f, 40.0f); break;
                    case 49: adjL(p.capacityMax,  0.1f,  1.0f,  0.0f, 9999.0f); break;
                    default: defaultL(); break;
                }
            }
        } else if (rightRel) {
            if (menuState == 40 && btnR.msCount < LONG_PRESS_TIME) {
                navRight(50, (ScreensEnum)6);
            } else if (menuState != 40) {
                storageDirty = true;
                switch (menuState) {
                    case 41: adjR(p.cruisePower,    (uint8_t)1, (uint8_t)5, (uint8_t)0, (uint8_t)100); break;
                    case 42: adjR(p.cruiseRampUp,   0.001f, 0.01f, 0.0f,  1.0f); break;
                    case 43: adjR(p.cruiseRampDown, 0.001f, 0.01f, 0.0f,  1.0f); break;
                    case 44: adjR(p.cruiseDeadband, 0.01f,  0.05f, 0.0f, 0.5f); break;
                    case 45: adjR(p.pasMode, (uint8_t)1, (uint8_t)1, (uint8_t)0, (uint8_t)3); break;
                    case 46: adjR(p.voltageMin,   0.1f,   1.0f,  0.0f, 99.9f); break;
                    case 47: adjR(p.voltageMax,   0.1f,   1.0f,  0.0f, 99.9f); break;
                    case 48: adjR(p.currentMax,   0.1f,   1.0f,  0.0f, 40.0f); break;
                    case 49: adjR(p.capacityMax,  0.1f,  1.0f,  0.0f, 9999.0f); break;
                    default: defaultR(); break;
                }
            }
        }

    } else if (menuState < 60) { // Screen 5: Misc (params 51-57)

        if (bothShort) {
            if (menuState == 50) {
                // Odo (laufender Gesamt-km-Stand) bleibt unberuehrt - nur der Trip wird
                // genullt. Es gilt weiterhin odo = alter Stand + tripKm.
                p.tripKm = 0.0f;
                calc_trip_reset();
                storageHotDirty = true;   // tripKm liegt im Hot-Datensatz
                clearBoth();
                modeDebounce = millis();
            } else {
                handleBothShort(50, p.pwRace ? 57 : 50);
            }
        } else if (bothLong) {
            if (p.pwRace) handleBothLong(50);
            else { menuState = 50; modeDebounce = millis(); }
        } else if (leftRel) {
            if (menuState == 50 && btnL.msCount < LONG_PRESS_TIME) {
                navLeft(40, (ScreensEnum)5);
            } else if (menuState != 50) {
                storageDirty = true;
                if (millis() - btnL.time1 > 400) btnL.released = 0;
                switch (menuState) {
                    case 51: adjL(p.wheelCircle,       (uint16_t)1,  (uint16_t)10, (uint16_t)0, (uint16_t)3000); break;
                    case 52: adjL(p.speedLimitStreet,  0.1f, 1.0f,  0.0f, 199.9f); break;
                    case 53: adjL(p.cruiseLimitStreet, 0.1f, 1.0f,  0.0f, 199.9f); break;
                    case 54: adjL(p.powerLimitStreet,  (uint16_t)1,  (uint16_t)10, (uint16_t)0, (uint16_t)5000); break;
                    case 55: adjL(p.speedLimitRace,    0.1f, 1.0f,  0.0f, 199.9f); break;
                    case 56: adjL(p.cruiseLimitRace,   0.1f, 1.0f,  0.0f, 199.9f); break;
                    case 57: adjL(p.powerLimitRace,    (uint16_t)1,  (uint16_t)10, (uint16_t)0, (uint16_t)5000); break;
                    default: defaultL(); break;
                }
            }
        } else if (rightRel) {
            if (menuState == 50 && btnR.msCount < LONG_PRESS_TIME) {
                navRight(60, (ScreensEnum)7);
            } else if (menuState != 50) {
                storageDirty = true;
                if (millis() - btnR.time1 > 400) btnR.released = 0;
                switch (menuState) {
                    case 51: adjR(p.wheelCircle,       (uint16_t)1,  (uint16_t)10, (uint16_t)0, (uint16_t)3000); break;
                    case 52: adjR(p.speedLimitStreet,  0.1f, 1.0f,  0.0f, 199.9f); break;
                    case 53: adjR(p.cruiseLimitStreet, 0.1f, 1.0f,  0.0f, 199.9f); break;
                    case 54: adjR(p.powerLimitStreet,  (uint16_t)1,  (uint16_t)10, (uint16_t)0, (uint16_t)5000); break;
                    case 55: adjR(p.speedLimitRace,    0.1f, 1.0f,  0.0f, 199.9f); break;
                    case 56: adjR(p.cruiseLimitRace,   0.1f, 1.0f,  0.0f, 199.9f); break;
                    case 57: adjR(p.powerLimitRace,    (uint16_t)1,  (uint16_t)10, (uint16_t)0, (uint16_t)5000); break;
                    default: defaultR(); break;
                }
            }
        }

    } else if (menuState < 70) { // Screen 6: Stopwatch (params 61=min, 62=h)

        if (bothShort) {
            if (menuState == 60) {
                if      (stopwatchState == 0) { stopwatchState = 1; }                  // Start
                else if (stopwatchState == 1) { stopwatchState = 2; }                  // Stop
                else                          { stopwatchState = 0; p.stopwatchTicks = 0; storageDirty = true; } // Reset
                clearBoth();
                modeDebounce = millis();
            } else {
                handleBothShort(60, 62, false); // Screen 6 hat nur Param 61 (min) + 62 (h)
            }
        } else if (bothLong) {
            handleBothLong(60);
        } else if (leftRel) {
            if (menuState == 60 && btnL.msCount < LONG_PRESS_TIME) {
                navLeft(50, (ScreensEnum)6);
            } else if (menuState != 60) {
                if (millis() - btnL.time1 > 400) btnL.released = 0;
                switch (menuState) {
                    case 61: // kurz=-1min, lang=Minuten schneller
                        if      (btnL.msCount < LONG_PRESS_TIME) { s.time = (s.time >= 60)   ? s.time-60   : s.time+86400-60;   ds3231_set(s.time); clearL(); }
                        else if (btnL.pressed)                    { s.time = (s.time >= 60)   ? s.time-60   : s.time+86400-60;   ds3231_set(s.time); btnL.released = 0; }
                        else                                      { clearL(); }
                        break;
                    case 62: // kurz=-1h, lang=Stunden schneller
                        if      (btnL.msCount < LONG_PRESS_TIME) { s.time = (s.time >= 3600) ? s.time-3600 : s.time+86400-3600; ds3231_set(s.time); clearL(); }
                        else if (btnL.pressed)                    { s.time = (s.time >= 3600) ? s.time-3600 : s.time+86400-3600; ds3231_set(s.time); btnL.released = 0; }
                        else                                      { clearL(); }
                        break;
                    default: defaultL(); break;
                }
            }
        } else if (rightRel) {
            if (menuState == 60 && btnR.msCount < LONG_PRESS_TIME) {
                navRight(70, (ScreensEnum)8);
            } else if (menuState != 60) {
                if (millis() - btnR.time1 > 400) btnR.released = 0;
                switch (menuState) {
                    case 61: // kurz=+1min, lang=Minuten schneller
                        if      (btnR.msCount < LONG_PRESS_TIME) { s.time = (s.time+60   < 86400) ? s.time+60   : s.time+60-86400;   ds3231_set(s.time); clearR(); }
                        else if (btnR.pressed)                   { s.time = (s.time+60   < 86400) ? s.time+60   : s.time+60-86400;   ds3231_set(s.time); btnR.released = 0; }
                        else                                     { clearR(); }
                        break;
                    case 62: // kurz=+1h, lang=Stunden schneller
                        if      (btnR.msCount < LONG_PRESS_TIME) { s.time = (s.time+3600 < 86400) ? s.time+3600 : s.time+3600-86400; ds3231_set(s.time); clearR(); }
                        else if (btnR.pressed)                   { s.time = (s.time+3600 < 86400) ? s.time+3600 : s.time+3600-86400; ds3231_set(s.time); btnR.released = 0; }
                        else                                     { clearR(); }
                        break;
                    default: defaultR(); break;
                }
            }
        }

    } else if (menuState < 80) { // Screen 7: Rear light (param 71)

        if (bothShort) {
            if (menuState == 70) {
                p.rearLightOnOff ^= 1;
                storageDirty = true;
                clearBoth();
                modeDebounce = millis();
            } else {
                handleBothShort(70, p.pwRace ? 72 : 71);
            }
        } else if (bothLong) {
            handleBothLong(70);
        } else if (leftRel) {
            if (menuState == 70 && btnL.msCount < LONG_PRESS_TIME) {
                navLeft(60, (ScreensEnum)7);
            } else if (menuState != 70) {
                storageDirty = true;
                if (millis() - btnL.time1 > 400) btnL.released = 0;
                switch (menuState) {
                    case 71: adjL(p.rearLightDim, (uint16_t)1, (uint16_t)5, (uint16_t)1, (uint16_t)100); break;
                    case 72: toggleL(p.streetOnRestart); break;
                    default: defaultL(); break;
                }
            }
        } else if (rightRel) {
            if (menuState == 70 && btnR.msCount < LONG_PRESS_TIME) {
                navRight(80, (ScreensEnum)9);
            } else if (menuState != 70) {
                storageDirty = true;
                if (millis() - btnR.time1 > 400) btnR.released = 0;
                switch (menuState) {
                    case 71: adjR(p.rearLightDim, (uint16_t)1, (uint16_t)5, (uint16_t)1, (uint16_t)100); break;
                    case 72: toggleR(p.streetOnRestart); break;
                    default: defaultR(); break;
                }
            }
        }

    } else if (menuState < 90) { // Screen 8: Front light (param 81)

        if (bothShort) {
            if (menuState == 80) {
                p.frontLightOnOff ^= 1;
                storageDirty = true;
                clearBoth();
                modeDebounce = millis();
            } else {
                handleBothShort(80, 83);
            }
        } else if (bothLong) {
            handleBothLong(80);
        } else if (leftRel) {
            if (menuState == 80 && btnL.msCount < LONG_PRESS_TIME) {
                navLeft(70, (ScreensEnum)8);
            } else if (menuState != 80) {
                storageDirty = true;
                if (millis() - btnL.time1 > 400) btnL.released = 0;
                switch (menuState) {
                    case 81: adjL(p.frontLightDim,    (uint16_t)1, (uint16_t)5,  (uint16_t)1, (uint16_t)100);  break;
                    case 82: adjL(p.backlightDim,     (uint16_t)1, (uint16_t)5,  (uint16_t)1, (uint16_t)101);  break;
                    case 83: adjL(p.onTimeBacklight,  (uint16_t)1, (uint16_t)10, (uint16_t)3, (uint16_t)1000); break;
                    default: defaultL(); break;
                }
            }
        } else if (rightRel) {
            if (menuState == 80 && btnR.msCount < LONG_PRESS_TIME) {
                navRight(0, (ScreensEnum)1);
            } else if (menuState != 80) {
                storageDirty = true;
                if (millis() - btnR.time1 > 400) btnR.released = 0;
                switch (menuState) {
                    case 81: adjR(p.frontLightDim,    (uint16_t)1, (uint16_t)5,  (uint16_t)1, (uint16_t)100);  break;
                    case 82: adjR(p.backlightDim,     (uint16_t)1, (uint16_t)5,  (uint16_t)1, (uint16_t)101);  break;
                    case 83: adjR(p.onTimeBacklight,  (uint16_t)1, (uint16_t)10, (uint16_t)3, (uint16_t)1000); break;
                    default: defaultR(); break;
                }
            }
        }
    }
}


