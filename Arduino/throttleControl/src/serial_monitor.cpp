#include <Arduino.h>
#include <stdarg.h>
#include "globals.h"
#include "menu.h"
#include "serial_monitor.h"
#include "vesc.h"   // lastVescValuesMs fuer VESC-Frame-Alter im JSON

// ---------------------------------------------------------------------------
// ANSI escape codes
// ---------------------------------------------------------------------------
#define NL   "\r\n"           // CRLF fuer Windows-Terminals (TeraTerm)
#define CLR  "\033[K"        // clear to end of line
#define RST  "\033[0m"       // reset
#define RED  "\033[1;31m"    // bold red  -> aktiver Edit-Parameter
#define CYN  "\033[36m"      // cyan      -> Seitentitel
#define YEL  "\033[1;33m"    // bold gelb -> RACE-Modus
#define BOL  "\033[1m"       // bold

// ---------------------------------------------------------------------------
// Styled value: gibt Wert in ROT aus wenn menuState == editCase
// ---------------------------------------------------------------------------
static void sv(uint8_t editCase, const char* fmt, ...) {
    char buf[32];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (menuState == editCase) Serial.print(RED);
    Serial.print(buf);
    if (menuState == editCase) Serial.print(RST);
}

// Zeile: "  Label:    Value\n" — Label linksbündig auf lw Zeichen
#define ROW(lw, label, fmt, ...) do { \
    Serial.printf("  %-*s", (int)(lw), label ":"); \
    Serial.printf(fmt CLR NL, ##__VA_ARGS__); \
} while(0)

// Zeile mit sv-Highlighting
#define ROWSV(lw, label, editCase, fmt, ...) do { \
    Serial.printf("  %-*s", (int)(lw), label ":"); \
    sv(editCase, fmt, ##__VA_ARGS__); \
    Serial.print(CLR NL); \
} while(0)

// ---------------------------------------------------------------------------
// Hilfsformatierungen
// ---------------------------------------------------------------------------
static void fmt_hms(char* buf, size_t n, uint32_t sec) {
    snprintf(buf, n, "%02lu:%02lu:%02lu",
             (unsigned long)(sec / 3600),
             (unsigned long)((sec % 3600) / 60),
             (unsigned long)(sec % 60));
}

static void fmt_hms_cs(char* buf, size_t n, uint32_t cs) {
    snprintf(buf, n, "%02lu:%02lu:%02lu.%02lu",
             (unsigned long)(cs / 360000),
             (unsigned long)((cs / 6000) % 60),
             (unsigned long)((cs / 100) % 60),
             (unsigned long)(cs % 100));
}

static void sep() {
    Serial.print("------------------------------------------------------------" NL);
}

static void blank() {
    Serial.print(CLR NL);
}

// ---------------------------------------------------------------------------
// Gemeinsamer Seitenkopf
// ---------------------------------------------------------------------------
static void print_header(const char* title, bool showCruise = false) {
    Serial.print(CYN BOL);
    Serial.printf("=== %-22s", title);
    Serial.print(RST);
    Serial.print("  Support: ");
    sv(1, "%u", p.supportLevel);
    if (showCruise) {
        Serial.print("  Cruise: ");
        if (s.cruise) Serial.print(RED "[ON]" RST);
        else          Serial.print("off");
    }
    Serial.print(CLR NL);
    sep();
}

// ---------------------------------------------------------------------------
// Live-Block (6 Werte) für Parameter-Pages 1–4
// ---------------------------------------------------------------------------
static void print_live() {
    ROW(14, "Speed",    "%.1f km/h", (double)s.speed);
    ROW(14, "Cadence",  "%.1f rpm",  (double)s.cadenceRpm);
    ROW(14, "Torque",   "%u",        s.torque);
    ROW(14, "Power",    "%u W",      s.power);
    ROW(14, "Current",  "%.1f A",    (double)s.current);
    ROW(14, "Throttle", "%.2f V",    (double)s.throttleVolt);
    sep();
}

// ---------------------------------------------------------------------------
// Page 0: MAIN
// ---------------------------------------------------------------------------
static void print_page_main() {
    char tb1[12], tb2[12];
    fmt_hms(tb1, sizeof(tb1), s.onTime);
    fmt_hms(tb2, sizeof(tb2), s.time);
    print_header("MAIN", true);
    Serial.printf("  %-14s", "Speed:");
    if (p.pwRace) Serial.printf("%.1f " RED "km/h" RST CLR NL, (double)s.speed);
    else          Serial.printf("%.1f km/h" CLR NL, (double)s.speed);
    ROW(14, "Cadence",  "%.1f rpm",  (double)s.cadenceRpm);
    ROW(14, "Torque",   "%u",        s.torque);
    ROW(14, "Power",    "%u W",      s.power);
    ROW(14, "Current",  "%.1f A",    (double)s.current);
    ROW(14, "IMotor",   "%.1f A",    (double)s.motorCurrent);   // Phasenstrom!
    ROW(14, "Duty",     "%.0f %%",   (double)(s.dutyCycle * 100.0f));
    ROW(14, "ERPM",     "%.0f",      (double)s.erpm);
    ROW(14, "Voltage",  "%.1f V",    (double)s.voltage);
    ROW(14, "Throttle", "%.2f V",    (double)s.throttleVolt);
    ROW(14, "TempMotor","%.1f C",    (double)s.tempMotor);
    ROW(14, "TempFet",  "%.1f C",    (double)s.tempFet);
    ROW(14, "Temp",     "%.1f C",    (double)s.temp);
    ROW(14, "Trip",     "%.1f km",   (double)p.tripKm);
    ROW(14, "Range",    "%u km",     s.range);
    ROW(14, "Wh/km",    "%.1f",      (double)s.whKm);
    ROW(14, "Capacity", "%u %%",     s.capacity);
    ROW(14, "SpeedAvg", "%.1f km/h", (double)s.speedAvg);
    ROW(14, "OnTime",   "%s",        tb1);
    ROW(14, "Time",     "%s",        tb2);
    ROW(14, "VESC age", "%lu ms",    (unsigned long)(millis() - lastVescValuesMs));
}

// ---------------------------------------------------------------------------
// Page 1: TORQUE PAR
// ---------------------------------------------------------------------------
static void print_page_torque() {
    print_header("TORQUE PAR", true);
    print_live();
    ROWSV(18, "TorqueZero",   11, "%u",    p.torqueZero);
    ROWSV(18, "TorqueMax",    12, "%u",    p.torqueMax);
    ROWSV(18, "Deadband",     13, "%u",    p.torqueDeadband);
    ROWSV(18, "FilterRise",   14, "%.3f",  p.torqueFilterRise);
    ROWSV(18, "FilterFall",   15, "%.3f",  p.torqueFilterFall);
    ROWSV(18, "TorqueIdle",   16, "%u ms", p.torqueIdleMs);
}

// ---------------------------------------------------------------------------
// Page 2: CADENCE PAR
// ---------------------------------------------------------------------------
static void print_page_cadence() {
    print_header("CADENCE PAR", true);
    print_live();
    ROWSV(18, "PulsesPerRev",  21, "%u",    p.pulsesPerRev);
    ROWSV(18, "TimeoutMs",     22, "%lu",   (unsigned long)p.cadenceTimeoutMs);
    ROWSV(18, "MaxRPM",        23, "%.1f",  p.cadenceMaxRpm);
    ROWSV(18, "FilterAlpha",   24, "%.3f",  p.cadenceFilterAlpha);
    ROWSV(18, "MaxLimit",      25, "%.1f",  p.cadenceMaxLimit);
    ROWSV(18, "GatePulses",    26, "%u",    p.cadenceGatePulses);
    ROWSV(18, "GateTime",      27, "%u ms", p.cadenceGateMs);
}

// ---------------------------------------------------------------------------
// Page 3: THROTTLE CURVE
// ---------------------------------------------------------------------------
static void print_page_curve() {
    print_header("THROTTLE CURVE", true);
    print_live();
    ROWSV(18, "CurveY25",      31, "%.3f",  p.curveY25);
    ROWSV(18, "CurveY50",      32, "%.3f",  p.curveY50);
    ROWSV(18, "CurveY75",      33, "%.3f",  p.curveY75);
    ROWSV(18, "CurveOffset",   34, "%.3f",  p.curveOffset);
    ROWSV(18, "RampUpLow",     35, "%.3f",  p.rampUpLow);
    ROWSV(18, "RampUpHigh",    36, "%.3f",  p.rampUpHigh);
    ROWSV(18, "RampThreshold", 37, "%.3f",  p.rampThreshold);
    ROWSV(18, "RampDown",      38, "%.3f",  p.rampDown);
}

// ---------------------------------------------------------------------------
// Page 4: PAS & BATTERY
// ---------------------------------------------------------------------------
static void print_page_pas_batt() {
    print_header("PAS & BATTERY", true);
    print_live();
    ROWSV(18, "CruisePower",   41, "%u %%",  p.cruisePower);
    ROWSV(18, "CruiseRampUp",  42, "%.3f",   p.cruiseRampUp);
    ROWSV(18, "CruiseRampDown",43, "%.3f",   p.cruiseRampDown);
    ROWSV(18, "CruiseDeadband",44, "%.2f",    p.cruiseDeadband);
    ROWSV(18, "PasMode",       45, "%u",     p.pasMode);

    ROWSV(18, "VoltageMin",    46, "%.1f V", p.voltageMin);
    ROWSV(18, "VoltageMax",    47, "%.1f V", p.voltageMax);
    ROWSV(18, "CurrentMax",    48, "%.1f A", p.currentMax);
    ROWSV(18, "CapacityMax",   49, "%.1f Wh",p.capacityMax);
}

// ---------------------------------------------------------------------------
// Page 5: DIV PARAMETERS
// ---------------------------------------------------------------------------
static void print_page_div() {
    print_header("DIV PARAMETERS", true);
    ROW  (18, "Trip",                 "%.1f km", (double)p.tripKm);
    ROW  (18, "Odo",                  "%lu km",  (unsigned long)p.odoKm);
    if (p.pwRace) ROWSV(18, "WheelCirc", 51, "%u mm", p.wheelCircle);
    if (p.pwRace) {
        sep();
        ROWSV(22, "SpeedLimit Street",  52, "%.1f km/h", p.speedLimitStreet);
        ROWSV(22, "CruiseLimit Street",53, "%.1f km/h", p.cruiseLimitStreet);
        ROWSV(22, "PowerLimit Street",  54, "%u W",      p.powerLimitStreet);
        blank();
        ROWSV(22, "SpeedLimit Race",    55, "%.1f km/h", p.speedLimitRace);
        ROWSV(22, "CruiseLimit Race",  56, "%.1f km/h", p.cruiseLimitRace);
        ROWSV(22, "PowerLimit Race",    57, "%u W",      p.powerLimitRace);
    }
}

// ---------------------------------------------------------------------------
// Page 6: STOPWATCH
// ---------------------------------------------------------------------------
static void print_page_stopwatch() {
    print_header("STOPWATCH", true);
    char sw[24], t[12];
    fmt_hms_cs(sw, sizeof(sw), p.stopwatchTicks);
    fmt_hms(t, sizeof(t), s.time);
    ROW(14, "Stopwatch", "%s", sw);
    if (menuState == 61 || menuState == 62)
        Serial.printf("  %-14s" RED "%s" RST CLR NL, "Time:", t);
    else
        ROW(14, "Time", "%s", t);
}

// ---------------------------------------------------------------------------
// Page 7: REAR LIGHT
// ---------------------------------------------------------------------------
static void print_page_rear() {
    print_header("REAR LIGHT", true);
    ROW  (20, "RearLight",  "%s",        p.rearLightOnOff ? "ON" : "off");
    ROWSV(20, "Dim",    71, "%u %%",     p.rearLightDim);
    ROW  (20, "IMU Ready",  "%s (%u samples)", s.imuReady ? "YES" : "NO", s.imuCalCount);
    ROW  (20, "AccForward", "%.2f m/s2  (Brake<%+.1f)", (double)s.accForward, (double)p.brakeThreshold);
    ROW  (20, "Braking",    "%s",        s.braking ? "YES" : "NO");
    ROW  (20, "rawAccX",    "%.2f",      (double)s.rawAccX);
    ROW  (20, "rawAccY",    "%.2f",      (double)s.rawAccY);
    ROW  (20, "rawAccZ",    "%.2f",      (double)s.rawAccZ);
    if (p.pwRace) { blank(); ROWSV(20, "StreetOnRestart", 72, "%s", p.streetOnRestart ? "YES" : "NO"); }
}

// ---------------------------------------------------------------------------
// Page 8: FRONT LIGHT / BACKLIGHT
// ---------------------------------------------------------------------------
static void print_page_front() {
    print_header("FRONT LIGHT", true);
    ROW(20, "FrontLight", "%s", p.frontLightOnOff ? "ON" : "off");
    ROWSV(20, "Dim", 81, "%u %%", p.frontLightDim);
    blank();
    // Backlight (Sonderwert 101 = AUTO)
    Serial.printf("  %-20s", "Backlight:");
    if (p.backlightDim == 101) {
        if (menuState == 82) Serial.print(RED "AUTO" RST); else Serial.print("AUTO");
    } else {
        sv(82, "%u %%", p.backlightDim);
    }
    Serial.print(CLR NL);
    // OnTime (Sonderwert 1000 = always ON)
    Serial.printf("  %-20s", "OnTime:");
    if (p.onTimeBacklight == 1000) {
        if (menuState == 83) Serial.print(RED "ON" RST); else Serial.print("ON");
    } else {
        sv(83, "%u sec", p.onTimeBacklight);
    }
    Serial.print(CLR NL);
}

// ---------------------------------------------------------------------------
// JSON-Ausgabe für Desktop-App (aktiviert mit '?', deaktiviert mit '!')
// ---------------------------------------------------------------------------
static void print_json() {
    // Live values
    Serial.printf("{\"sp\":%.1f,\"cd\":%.1f,\"tq\":%u,\"pw\":%u,",
        (double)s.speed, (double)s.cadenceRpm, s.torque, s.power);
    Serial.printf("\"cu\":%.1f,\"vo\":%.1f,\"th\":%.2f,\"tp\":%.1f,",
        (double)s.current, (double)s.voltage, (double)s.throttleVolt, (double)s.temp);
    // Diagnostic: rawNorm, filteredTorque (Anzeige), gatedTorque (Regelung), target, cadenceGateOpen
    Serial.printf("\"drn\":%.3f,\"dft\":%.3f,\"dgf\":%.3f,\"dgt\":%u,\"dtg\":%.3f,",
        (double)s.diagRawNorm, (double)s.diagFilteredTorque, (double)s.diagGatedTorque,
        s.diagCadenceGateOpen, (double)s.diagTarget);
    // VESC: Phasenstrom, Duty, ERPM, Temperaturen
    Serial.printf("\"imot\":%.2f,\"duty\":%.3f,\"erpm\":%.0f,\"tmot\":%.1f,\"tfet\":%.1f,",
        (double)s.motorCurrent, (double)s.dutyCycle, (double)s.erpm,
        (double)s.tempMotor, (double)s.tempFet);
    Serial.printf("\"tr\":%.1f,\"od\":%lu,\"ra\":%u,\"wh\":%.1f,",
        (double)p.tripKm, (unsigned long)p.odoKm, s.range, (double)s.whKm);
    Serial.printf("\"ca\":%u,\"sa\":%.1f,\"ot\":%lu,\"t\":%lu,",
        s.capacity, (double)s.speedAvg,
        (unsigned long)s.onTime, (unsigned long)s.time);
    Serial.printf("\"rc\":%u,\"su\":%u,\"sw\":%lu,\"ss\":%u,\"crs\":%u,\"pg\":%u,\"ms\":%u,",
        p.pwRace, p.supportLevel,
        (unsigned long)p.stopwatchTicks, stopwatchState,
        s.cruise, (uint8_t)(menuState / 10), menuState);
    // Torque parameters
    Serial.printf("\"tz\":%u,\"tm\":%u,\"tdb\":%u,\"tfr\":%.3f,\"tff\":%.3f,\"tidl\":%u,",
        p.torqueZero, p.torqueMax, p.torqueDeadband,
        (double)p.torqueFilterRise, (double)p.torqueFilterFall, p.torqueIdleMs);
    // Cadence parameters
    Serial.printf("\"ppr\":%u,\"cto\":%lu,\"crpm\":%.1f,\"cfa\":%.3f,\"cml\":%.1f,",
        p.pulsesPerRev, (unsigned long)p.cadenceTimeoutMs,
        (double)p.cadenceMaxRpm, (double)p.cadenceFilterAlpha,
        (double)p.cadenceMaxLimit);
    Serial.printf("\"cgp\":%u,\"cgt\":%u,", p.cadenceGatePulses, p.cadenceGateMs);
    // Curve parameters
    Serial.printf("\"cy25\":%.3f,\"cy50\":%.3f,\"cy75\":%.3f,\"cyo\":%.3f,",
        (double)p.curveY25, (double)p.curveY50, (double)p.curveY75, (double)p.curveOffset);
    Serial.printf("\"rul\":%.4f,\"ruh\":%.4f,\"rth\":%.3f,\"rdn\":%.4f,",
        (double)p.rampUpLow, (double)p.rampUpHigh,
        (double)p.rampThreshold, (double)p.rampDown);
    // Cruise & Battery parameters
    Serial.printf("\"cp\":%u,\"cru\":%.4f,\"crd\":%.4f,\"cdb\":%.2f,\"pmd\":%u,",
        p.cruisePower, (double)p.cruiseRampUp, (double)p.cruiseRampDown,
        (double)p.cruiseDeadband, p.pasMode);
    Serial.printf("\"vmn\":%.1f,\"vmx\":%.1f,\"imx\":%.1f,\"cpmx\":%.1f,",
        (double)p.voltageMin, (double)p.voltageMax,
        (double)p.currentMax, (double)p.capacityMax);
    // Div parameters
    Serial.printf("\"wc\":%u,\"sls\":%.1f,\"cls\":%.1f,\"pls\":%u,",
        p.wheelCircle, (double)p.speedLimitStreet,
        (double)p.cruiseLimitStreet, p.powerLimitStreet);
    Serial.printf("\"slr\":%.1f,\"clr\":%.1f,\"plr\":%u,",
        (double)p.speedLimitRace, (double)p.cruiseLimitRace, p.powerLimitRace);
    // Light parameters
    Serial.printf("\"rlon\":%u,\"rld\":%u,\"sor\":%u,\"brk\":%u,",
        p.rearLightOnOff, p.rearLightDim, p.streetOnRestart, (uint8_t)s.braking);
    Serial.printf("\"flon\":%u,\"fld\":%u,\"bld\":%u,\"otb\":%u}\r\n",
        p.frontLightOnOff, p.frontLightDim, p.backlightDim, p.onTimeBacklight);
}

// ---------------------------------------------------------------------------
// Öffentliche Funktion — aus loop() aufrufen
// ---------------------------------------------------------------------------
void serial_update() {
    static bool     jsonMode = false;
    static uint32_t lastMs   = 0;
    static uint8_t  lastPage = 0xFF;

    while (Serial.available()) {
        char c = (char)Serial.read();
        if      (c == '?') { jsonMode = true; lastMs = 0; }
        else if (c == '!') { jsonMode = false; Serial.write("\033[2J"); lastPage = 0xFF; }
    }

    uint32_t interval = jsonMode ? 20 : 200;  // JSON: 50 Hz, ANSI: 5 Hz
    if (millis() - lastMs < interval) return;
    lastMs = millis();

    if (jsonMode) {
        print_json();
        return;
    }

    uint8_t page = menuState / 10;

    // Bei Seitenwechsel: ganzen Bildschirm löschen (verhindert Geisterzeilen)
    if (page != lastPage) {
        Serial.write("\033[2J");
        lastPage = page;
    }

    Serial.write("\033[?25l");  // Cursor verstecken
    Serial.write("\033[H");     // Cursor Home

    switch (page) {
        case 0: print_page_main();        break;
        case 1: print_page_torque();      break;
        case 2: print_page_cadence();     break;
        case 3: print_page_curve();       break;
        case 4: print_page_pas_batt(); break;
        case 5: print_page_div();         break;
        case 6: print_page_stopwatch();   break;
        case 7: print_page_rear();        break;
        case 8: print_page_front();       break;
        default:
            Serial.printf("  Unbekannte Page %u" CLR NL, page);
            break;
    }

    // Alles unterhalb des aktuellen Inhalts löschen
    Serial.print("\033[J");
}
