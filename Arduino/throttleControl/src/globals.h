#pragma once

#include <Wire.h>
#include <lvgl.h>
#include "pico/mutex.h"

// ---------------------------------------------------------------------------
// Storage-Auswahl: genau eine Option aktiv lassen
// ---------------------------------------------------------------------------
#define STORAGE_FRAM   0
#define STORAGE_EEPROM 1
#define STORAGE_TYPE   STORAGE_FRAM   // Standard: FRAM
// FRAM wird direkt ueber Wire2 angesprochen (siehe storage.cpp) -> keine externe Library.

// --- PIN DEFINITIONEN ---

#define LED_BOARD       20
#define NUM_LEDS        1

#define TORQUE_PIN      26
#define CADENCE_PIN     6
#define SPEED_PIN       7 //14
#define CURRENT_PIN     27
#define VOLTAGE_PIN     28
#define CONTROL_OUT_PIN 25
#define CONTROL_IN_PIN  29

#define BACKLIGHT_PIN   22
#define FRONT_LIGHT_PIN 23
#define REAR_LIGHT_PIN  24

#define TOUCH_L_PIN     10
#define TOUCH_R_PIN     11

#define TFT_MOSI_PIN    19 //7
#define TFT_MISO_PIN    16
#define TFT_SCK_PIN     18
#define TFT_DC_PIN      13
#define TFT_CS_PIN      17

#define RESET_PIN       21
#define CRUISE_PIN      12
#define SDA_PIN         4
#define SCL_PIN         5

// --- DATENSTRUKTUREN ---

struct EBikeStatus {
  uint32_t time = 0;
  float temp = 0.0;
  volatile float speed = 0.0;
  uint16_t range = 0;
  uint8_t capacity = 0;
  float whKm = 0.0;
  // voltage/current/power: von Core 0 (VESC/calc) geschrieben, von Core 1
  // (loop1 Power-/Strom-Limit) gelesen -> volatile (aligned 32-bit = atomar auf M33)
  volatile float voltage = 0.0;
  volatile float current = 0.0;
  volatile uint16_t torque = 0;
  volatile uint16_t power = 0;
  volatile float cadence = 0.0;    // normiert 0–1 (intern für Berechnung)
  volatile float cadenceRpm = 0.0; // tatsächliche RPM (Core 1 -> Anzeige Core 0)
  float speedAvg = 0.0;
  float tripTimeH = 0.0;
  // throttleVolt/cruise: von Core 1 (loop1) geschrieben, von Core 0 (UI/Serial) gelesen
  // -> volatile. Ohne das cacht der Compiler den Wert und die Anzeige friert ein.
  volatile float throttleVolt = 0.0;
  uint32_t onTime = 0;
  volatile uint8_t cruise = 0;
  float accForward = 0.0f;
  bool braking = false;
  bool imuReady = false;
  uint8_t imuCalCount = 0;
  float rawAccX = 0.0f;
  float rawAccY = 0.0f;
  float rawAccZ = 0.0f;
  uint8_t  dbgFrameCmd = 0;
  uint8_t  dbgFrameLen = 0;
  uint8_t  dbgFrameCount = 0;
  uint8_t  dbgBytes[8] = {};
  uint8_t  dbgImuLen = 0;   // letzter empfangener IMU-Payload (0 = noch kein Frame)
  // Diagnostic (Core 1 -> Anzeige/Serial, für Ruck-Analyse)
  volatile float diagRawNorm = 0.0f;
  volatile float diagFilteredTorque = 0.0f;
  volatile float diagTarget = 0.0f;
  volatile uint8_t diagCadenceGateOpen = 0;
  // VESC-Telemetrie, die bisher verworfen wurde. motorCurrent ist der PHASENSTROM
  // (nicht der Batteriestrom!) - er bestimmt das Drehmoment und damit die Last am
  // Getriebe. Bei kleiner Duty ist er ein Vielfaches von current.
  volatile float motorCurrent = 0.0f;  // avg_motor_current [A]
  volatile float dutyCycle = 0.0f;     // -1.0 .. 1.0
  volatile float erpm = 0.0f;          // elektrische U/min
  volatile float tempMotor = 0.0f;     // [°C], nur gueltig mit angeschlossenem Sensor
  volatile float tempFet = 0.0f;       // [°C] Controller
};

// WICHTIG: Alle Felder, die von Core 1 (loop1/applyCurve/ISRs) gelesen und vom Menue
// auf Core 0 geschrieben werden, MUESSEN volatile sein - sonst cacht der Compiler den
// Wert auf Core 1 und Menue-Aenderungen wirken nicht (oder erst zufaellig). Alle sind
// <=32 Bit und aligned -> auf dem M33 atomar, kein Tearing, keine Mutex noetig.
// Reine Core-0-Felder (Licht, Akku-Kapazitaet, Odo, Stopwatch, ...) brauchen es nicht.
struct EBikeSettings {
  volatile uint8_t supportLevel = 12;

  volatile uint16_t torqueZero = 3890;      // ADC bei 0 Kraft (~2.85V @ 3V Ref, invertierter Sensor)
  volatile uint16_t torqueMax = 478;        // ADC bei Volllast (~0.35V @ 3V Ref)
  volatile uint16_t torqueDeadband = 600;   // 137 = ~0.1V Schwelle unter torqueZero
  volatile float torqueFilterRise = 0.450;
  volatile float torqueFilterFall = 0.015;

  // Pedal-Timeout [ms]: Liegt laenger als das kein Drehmoment ueber der Deadband-
  // Schwelle an, tritt niemand -> Motor aus. Erkennung ueber das DREHMOMENT und NICHT
  // ueber die Kadenz, weil der PAS (DM02) mit dem Antrieb mitdreht und weiterzaehlt,
  // auch wenn nicht getreten wird. Das Fenster ueberbrueckt die Totpunkte der
  // Pedalumdrehung. Kleiner = schnelleres Abschalten, aber irgendwann Ruckeln. 0-999.
  //
  // ACHTUNG - das ist keine freie Wahl: Das Drehmoment hat ZWEI Spitzen pro Kurbel-
  // umdrehung (ein Bein pro Halbdrehung). Der Abstand dazwischen ist
  //   Totpunktabstand [ms] = 30000 / Trittfrequenz
  // Ist torqueIdleMs kleiner, laeuft der Timer im Totpunkt ab und der Antrieb wird
  // ueber den Cutoff in loop1 SCHLAGARTIG abgeschaltet - bei voller Leistung ein
  // Schlag in den Antriebsstrang. 400 ms entsprachen 75 U/min, also mitten im
  // normalen Trittbereich. 900 ms decken bis hinunter zu 33 U/min ab.
  volatile uint16_t torqueIdleMs = 900;

  volatile uint8_t pulsesPerRev = 12;   // wird im cadenceInterrupt (Core 1) gelesen
  volatile uint32_t cadenceTimeoutMs = 500;  // Verfall des MESSWERTS -> Kadenz = 0.
                                             // Betrifft Modus 0/1 (dort ist die Kadenz die
                                             // Hoehe) und die Anzeige. KEIN Gate.
  volatile float cadenceMaxRpm = 120.0;
  volatile float cadenceFilterAlpha = 0.08;
  volatile float cadenceMaxLimit = 150.0;

  // --- Kadenz-Gate: NUR pasMode 3 (dort ist die Kadenz ein reines Ein/Aus-Gate) ------
  // Bewusst von cadenceTimeoutMs entkoppelt, damit ein grosszuegiges Gate-Fenster nicht
  // die Kadenz-Hoehe in Modus 0/1 oder die Anzeige nachlaufen laesst.
  // Das Gate kennt KEINE Mindest-Drehzahl mehr (frueher hart 0.1 * cadenceMaxRpm =
  // 12 U/min -> am Berg gab es unter 12 U/min gar keine Unterstuetzung).
  //
  // Wie viele AUFEINANDERFOLGENDE Pulse oeffnen das Gate. 1 = Freigabe schon nach
  // 360/pulsesPerRev Grad Kurbeldrehung (bei 12 Magneten 30 Grad) -> spritzig.
  // Groesser = traeger, aber immun gegen einzelne Stoerpulse (Ruetteln, Transport).
  volatile uint8_t cadenceGatePulses = 1;    // 1-12
  // Wie lange das Gate nach dem LETZTEN Puls offen bleibt [ms]. Reisst die Pulsfolge
  // laenger ab, schliesst das Gate und der Pulszaehler faellt auf 0 zurueck.
  // Bestimmt die langsamste Trittfrequenz, bei der das Gate lueckenlos offen bleibt:
  //   RPM_min = 60000 / (cadenceGateMs * pulsesPerRev)   -> 1000ms/12 Magnete = 5 U/min
  volatile uint16_t cadenceGateMs = 1000;    // 100-3000

  // Stuetzstellen der Unterstuetzungskennlinie (applyCurve, 4 lineare Abschnitte).
  // Entscheidend ist nicht die Hoehe, sondern dass die STEIGUNGEN monoton bleiben:
  //   0.15/0.35/0.65 -> 0.6 / 0.8 / 1.2 / 1.4  (progressiv, Verhaeltnis 2.3:1)
  // Zum Vergleich 0.20/0.30/0.90 -> 0.8 / 0.4 / 2.4 / 0.4: der Abschnitt zwischen
  // 50 % und 75 % Pedalkraft war SECHSMAL steiler als seine Nachbarn. Genau dort
  // wandert das gefilterte Drehmoment bei jedem Tritt durch -> die Unterstuetzung
  // schwankte um den Faktor drei (spuerbares Ruckeln, CSV-Analyse 08/2026).
  // Ist es insgesamt zu kraeftig: supportLevel senken, nicht den Knick wieder einbauen -
  // supportLevel multipliziert linear und laesst die Form der Kurve intakt.
  volatile float curveY25 = 0.15; //linear = 0.25
  volatile float curveY50 = 0.35; //linear = 0.5
  volatile float curveY75 = 0.65; //linear = 0.75
  // Gleiche Rampe ober- und unterhalb von rampThreshold: ein Sprung an der Schwelle
  // erzeugt beim Wiederangasen einen spuerbaren Ruck.
  volatile float rampUpLow = 0.010;
  volatile float rampUpHigh = 0.010;
  volatile float rampThreshold = 0.5;
  volatile float rampDown = 0.030;
  volatile float curveOffset = 0.0;

  volatile uint8_t cruisePower = 40;
  volatile float cruiseRampUp = 0.001;
  volatile float cruiseRampDown = 0.001; // Ramp-Down in pasMode 2 (greift auch beim Loslassen
                                // der Cruise-Taste, wo cruisePressed schon false ist)
  // Cruise-Deadband: Totzone unterhalb von cruiseLimit, in der der Cruise-Integrator
  // den Throttle HAELT (weder hoch- noch runterrampt) -> er pendelt sich auf einem Wert
  // ein. Hysterese gegen Zappeln; noetig, weil die Speed-Messung bei Schiebehilfe-Tempo
  // sehr traege ist (1 Puls/Radumdrehung ~1.4 s bei 6 km/h).
  // ANTEIL von cruiseLimit (dimensionslos), nicht km/h -> skaliert automatisch mit dem
  // Limit mit. 0.10 = 10% (bei cruiseLimit 6 km/h also 0.6 km/h). Bereich 0.0-0.5;
  // >0.5 waere sinnlos, da der Throttle sonst kaum noch hochrampen kann.
  volatile float cruiseDeadband = 0.10f;
  // pasMode: 0 = Drehmoment (Kadenz nur als Ein/Aus-Gate)   <- Standard
  //          1 = Drehmoment x Kadenz (beide bestimmen die Hoehe)
  //          2 = nur Kadenz bestimmt die Hoehe
  //          3 = nur Cruise (ohne Taste kein Antrieb)
  // In 0/1/2 ist das DREHMOMENT immer das Haupt-Ein/Aus-Kriterium (torqueIdleMs).
  // Die Cruise-Taste wirkt in ALLEN Modi und uebersteuert das PAS-Ziel (begrenzt durch
  // cruiseLimit und cruisePower).
  volatile uint8_t pasMode = 0;
  float voltageMin = 39.0;
  float voltageMax = 58.8;
  volatile float currentMax = 15.0;
  float capacityMax = 800.0f; // Akkukapazität [Wh]

  float tripKm = 0.0;

  volatile uint16_t wheelCircle = 2300;   // wird im speedInterrupt (Core 1) gelesen
  volatile float speedLimitStreet = 25.0;
  volatile float speedLimitRace = 150.0;
  volatile float cruiseLimitStreet = 6.0;
  volatile float cruiseLimitRace = 6.0;
  volatile uint16_t powerLimitStreet = 250;
  volatile uint16_t powerLimitRace = 2000;

  uint32_t stopwatchTicks = 0;

  uint16_t rearLightDim = 20;
  uint8_t rearLightOnOff = 0;

  uint16_t frontLightDim = 20;
  uint8_t frontLightOnOff = 0;

  uint16_t backlightDim = 100; //101 = AUTO, 0-100%
  uint16_t onTimeBacklight = 1000; //1000 = ON, 0-999 sec.

  uint8_t streetOnRestart = 1; //1 = YES, 0 = NO

  volatile uint8_t pwRace = 1; //pwRace 1 = raceMode, 0 = streetMode

  float tempCalOffset = 2.00f; // Kalibrierungsoffset Temperatursensor [°C]

  uint16_t brakeHoldMs = 500;      // Bremslicht Nachleuchtzeit [ms]
  float brakeThreshold = -1.5f;   // Bremserkennung Schwellwert [m/s²] (negativer Wert)

  float controlThresholdV = 2.55f; // CONTROL_IN_PIN Schwellspannung am ADC-Pin [V] (0–3.0V)

  float odoKm = 0.0f;        // Gesamtkilometer (persistent, float -> exakt, kein km-Verlust).
                             // Zaehlt laufend mit tripKm mit -> es gilt immer
                             // odo = alter Stand + tripKm. Anzeige gerundet in ganzen km.
  float lastWhKm = 15.0f;   // Wh/km aus letzter Session (Startwert für Range)
};

// --- GLOBALE INSTANZEN ---

extern EBikeSettings p;
extern EBikeStatus   s;

extern TwoWire Wire2;
extern bool storageDirty;     // Settings geaendert -> kompletter Block faellig
extern bool storageHotDirty;  // Trip/Odo geaendert -> nur der Hot-Datensatz faellig
extern uint32_t lastButtonActivityMs;

// ADC ist ein einziges Peripheral mit gemeinsamem Mux -> Zugriffe von beiden
// Cores (TORQUE auf Core 1, CONTROL_IN auf Core 0) ueber Mutex serialisieren.
int analogReadShared(uint8_t pin);
