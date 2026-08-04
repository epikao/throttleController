#include <Arduino.h>
#include <Wire.h>

#include "hardware/gpio.h"
#include "hardware/regs/sio.h"
#include "hardware/structs/sio.h"
#include "pico/mutex.h"

#include "src/button.h"
#include "src/storage.h"
#include "src/lights.h"
#include "src/calc.h"
#include "src/vesc.h"
#include "src/ds3231.h"
#include "src/mcp9808.h"
#include "src/menu.h"
#include "src/veml7700.h"
#include "src/tft.h"
#include "src/ui/ui.h"
#include "src/ui/actions.h"
#include "src/globals.h"
#include "src/ui_update.h"
#include "src/racekey.h"
#include "src/serial_monitor.h"
#include "src/control.h"
#include "src/i2c_scanner.h"
#include "src/ws2812.h"

const uint8_t DAC_ADDR = 0x65; // MCP4725A2, A0=5V (per I2C-Scan bestaetigt). NICHT 0x60 -
                               // der gekaufte Chip ist ein A2-Typ; der genutzte hat A0 an 5V.
                               // (Der zweite DAC am Bus mit A0=GND liegt auf 0x64, ungenutzt.)

// Throttle-Ausgang: MCP4725 wird mit DAC_VREF_V versorgt (= Vollausschlag bei Code
// 4095). Der VESC-ADC vertraegt aber max. 3.3V -> Vollausschlag auf THROTTLE_MAX_V
// begrenzen. Puffer unter 3.3V, da der reale DAC-Vollausschlag von der tatsaechlichen
// Versorgungsspannung abhaengt. NIE ueber 3.3V setzen (VESC-Pin-Schaden!).
#define DAC_VREF_V     5.00f
#define THROTTLE_MAX_V 4.80f

// Watchdog scharfschalten? 1 = Betrieb (Sicherheit), 0 = Entwicklung.
// ACHTUNG: Aktiver Hardware-Watchdog verhindert das Flashen per 1200-Baud-Touch
// (Auto-Reset), weil er den Chip aus dem UF2-Bootloader zurueckresettet -> dann
// BOOTSEL-Taste noetig. Fuer bequemes Flashen auf 0, fuer den finalen Build auf 1.
#define ENABLE_WATCHDOG 0

// EINMALIG Odo (und Trip) loeschen. Ablauf:
//   1 setzen -> kompilieren -> flashen -> einmal booten lassen (Odo steht auf 0)
//   -> WIEDER AUF 0 setzen -> erneut flashen.
// ACHTUNG: Bleibt der Wert auf 1, wird Odo bei JEDEM Start wieder genullt.
// Das Nullen passiert NACH storage_load(), sonst wuerde der geladene Wert es ueberschreiben.
#define RESET_ODO_ONCE 0

// Status-LED (WS2812 auf GP20) aktiv? 1 = normal, 0 = komplett aus (kein PIO-Code laeuft).
// DIAGNOSE: Auf 0 setzen, um auszuschliessen, dass der WS2812/PIO-Treiber Core 0 stoert.
#define ENABLE_STATUS_LED 1

// I2C-Scan-Modus (Diagnose): 1 = beim Boot beide Busse scannen und die gefundenen
// Adressen dauerhaft ueber Serial ausgeben (Motorsteuerung + Dashboard laufen dann NICHT):
//   Core 0 -> Wire2 (Sensor-/Storage-Bus: MCP9808-Temp 0x18-0x1F, FRAM 0x50, DS3231, VEML7700)
//   Core 1 -> Wire  (DAC-Bus/I2C0, GP4/GP5: MCP4725)
// 0 = normaler Betrieb. Nach der Diagnose wieder auf 0 setzen.
#define I2C_SCAN_MODE 0

// Watchdog-Timeout [ms]: loop1 (Core 1) laeuft mit 100 Hz und fuettert jeden
// Durchlauf. Wird die Echtzeit-Steuerung > WDT_TIMEOUT_MS blockiert -> Chip-Reset.
#define WDT_TIMEOUT_MS 200

// Speed-Taper [km/h]: Band UNTERHALB des Speed-Limits, ueber das die Unterstuetzung
// gleitend auf 0 ausgeblendet wird - statt am Limit hart abzuschalten. Der harte Cutoff
// erzeugt dasselbe Bang-Bang-Pumpen wie frueher beim Cruise: Ziel springt zwischen
// vollem PAS-Wert und 0. Bei exakt speedLimit ist die Unterstuetzung 0 (gesetzeskonform).
#define SPEED_TAPER_KMH 1.5f

// Speed-Timeout [ms]: Bis dahin wird der letzte Messwert GEHALTEN (Standard-Verhalten
// eines E-Bike-Tachos), danach gilt Stillstand -> 0.
// Legt zugleich die kleinste messbare Geschwindigkeit fest:
//   v_min = wheelCircle * 3.6 / SPEED_TIMEOUT_MS   -> 2300mm / 2500ms = 3.3 km/h
#define SPEED_TIMEOUT_MS 2500

// Korrektur-Takt des Cruise-Reglers [ms]. loop1 laeuft mit 100 Hz, die Speed-Messung
// liefert aber nur alle ~1.4 s einen neuen Wert (1 Puls/Radumdrehung bei 6 km/h).
// Mit 100 Hz wuerde der Integrator bis zu 138x auf DENSELBEN veralteten Messwert
// reagieren -> massive Ueberkorrektur (Gas viel zu schnell weg), und zwar unabhaengig
// davon, wie klein cruiseRampDown eingestellt ist.
// Effektive Rate = cruiseRampDown / (CRUISE_CORRECT_MS/1000) [Throttle pro Sekunde].
// Der Aufbau aus dem Stand laeuft weiterhin kontinuierlich (100 Hz) - nur die
// Korrektur auf eine Messung wird getaktet.
#define CRUISE_CORRECT_MS 250

// Speed-Sensor: Plausibilitaetsgrenze leitet sich aus dem hoechsten konfigurierten
// Speed-Limit ab (zzgl. Marge fuer Bergab-Rollen). Pulse mit kuerzerem Abstand
// werden als Prellen/Doppelflanke verworfen (siehe speedInterrupt). So bleibt das
// Glitch-Fenster eng, erlaubt aber die Anzeige bis zum eingestellten Limit.
#define SPEED_PLAUSIBLE_MARGIN 1.10f  // 10% Headroom ueber dem hoechsten Limit

bool wsLed_state = false;
auto_init_mutex(led_mutex);

void lv_log_cb(const char * buf);
void wsLed_toggle_handler();
inline void fastToggle(uint pin);

// Globaler Zugriff für beide Cores
EBikeSettings p;
EBikeStatus s;
bool storageDirty = false;
bool storageHotDirty = false;
uint32_t lastButtonActivityMs = 0;

// Zustandsvariablen (Core 1)
volatile float instantRpm = 0;
volatile uint32_t lastPulseUs = 0;
volatile uint32_t lastPulseMs = 0;
volatile uint8_t  cadencePulseCount = 0; // aufeinanderfolgende Kadenz-Pulse (Gate, pasMode 3)

volatile uint32_t lastSpeedUs = 0;
volatile uint32_t lastSpeedMs = 0;

float filteredTorque = 0;
float filteredCadence = 0;
float throttle = 0;

// --- HILFSFUNKTIONEN ---

// Hoechstes konfiguriertes Speed-Limit (Street/Race/Cruise) + Marge [km/h].
// Bestimmt das Entprell-Fenster des Speed-Sensors und die anzeigbare Maximalspeed.
static inline float speedPlausibleMaxKmh() {
  float m = p.speedLimitStreet;
  if (p.speedLimitRace    > m) m = p.speedLimitRace;
  if (p.cruiseLimitStreet > m) m = p.cruiseLimitStreet;
  if (p.cruiseLimitRace   > m) m = p.cruiseLimitRace;
  return m * SPEED_PLAUSIBLE_MARGIN;
}

float applyCurve(float input) {
  input = constrain(input, 0.0f, 1.0f);
  if (input <= 0.25f) return (input / 0.25f) * p.curveY25;
  if (input <= 0.50f) return p.curveY25 + ((input - 0.25f) / 0.25f) * (p.curveY50 - p.curveY25);
  if (input <= 0.75f) return p.curveY50 + ((input - 0.50f) / 0.25f) * (p.curveY75 - p.curveY50);
  return p.curveY75 + ((input - 0.75f) / 0.25f) * (1.0f - p.curveY75);
}

void sendToDAC(uint16_t val) {
    Wire.beginTransmission(DAC_ADDR);
  Wire.write(0x40);
  Wire.write(val >> 4);
  Wire.write((val << 4) & 0xF0);
  Wire.endTransmission();
}

void cadenceInterrupt() {
  uint32_t nowUs = micros();
  uint32_t diff = nowUs - lastPulseUs;
  if (diff > 5000) {
    instantRpm = (60000000.0f) / (float)(diff * p.pulsesPerRev);
    lastPulseUs = nowUs;
    lastPulseMs = millis();
    // Zaehler fuer das Kadenz-Gate (pasMode 3). Zaehlt AUFEINANDERFOLGENDE Pulse;
    // loop1 setzt ihn zurueck, sobald die Pulsfolge laenger als cadenceGateMs abreisst.
    // Wichtig: schon der ERSTE Puls zaehlt - anders als instantRpm, das erst ab dem
    // zweiten Puls einen gueltigen Wert liefert (Intervall-Messung).
    if (cadencePulseCount < 255) cadencePulseCount++;
  }
}

void speedInterrupt() {
  uint32_t nowUs = micros();
  uint32_t diff = nowUs - lastSpeedUs;
  if (p.wheelCircle == 0) return;
  // Plausibilitaetsgrenze: Pulsabstaende, die einer unrealistischen
  // Geschwindigkeit entsprechen, sind Prellen/Doppelflanken -> verwerfen.
  // lastSpeedUs NICHT updaten, damit der naechste echte Puls korrekt misst.
  float maxKmh = speedPlausibleMaxKmh();
  if (maxKmh <= 0.0f) return;
  uint32_t minDiffUs = (uint32_t)((float)p.wheelCircle * 3600.0f / maxKmh);
  if (diff < minDiffUs) return;
  // wheelCircle [mm], diff [us] -> km/h
  s.speed = (float)p.wheelCircle * 3600.0f / (float)diff;
  lastSpeedUs = nowUs;
  lastSpeedMs = millis();
}

// ================================================================
// CORE 0: UI & PARAMETER-ANPASSUNG
// ================================================================
void setup() {
  // Selbsthaltung ZUERST setzen: haelt die Versorgung unabhaengig vom Akku-Knopf.
  // Muss vor den Verbrauchern (Backlight/Lichter) stehen, damit deren Einschalt-
  // strom die schwach gespeiste Schiene nicht einbrechen laesst, bevor gelatcht ist.
  pinMode(CONTROL_OUT_PIN, OUTPUT); digitalWrite(CONTROL_OUT_PIN, HIGH);
  pinMode(CONTROL_IN_PIN,  INPUT);
  pinMode(BACKLIGHT_PIN,   OUTPUT); digitalWrite(BACKLIGHT_PIN,   HIGH);
  pinMode(FRONT_LIGHT_PIN, OUTPUT); digitalWrite(FRONT_LIGHT_PIN, HIGH);
  pinMode(REAR_LIGHT_PIN,  OUTPUT); digitalWrite(REAR_LIGHT_PIN,  HIGH);
  // XL4001-Buck kann per EN-PWM nur bei niedriger Frequenz dimmen (bei 50kHz glaettet
  // der Ausgang alles weg -> nur an/aus). ~200 Hz: Regler folgt, kaum sichtbares Flackern.
  analogWriteFreq(200);

  Serial.begin(115200);

  Wire2.begin();

#if I2C_SCAN_MODE
  // Diagnose: BEIDE I2C-Busse nacheinander von DIESEM einen Core scannen -> saubere,
  // nicht verschachtelte Ausgabe. Blockiert HIER, bevor loop() die Busse nutzt. Core 1
  // haelt im Scan-Modus komplett still (siehe setup1) -> kein zweiter Master, keine
  // Kollision. Wire (i2c0, GP4/GP5) wird dafuer hier mitinitialisiert (sonst macht das
  // erst Core 1).
  Wire.setSDA(SDA_PIN);
  Wire.setSCL(SCL_PIN);
  Wire.begin();
  Wire.setClock(100000);
  delay(300);
  while (1) {
    Serial.print("\033[2J\033[H");  // Terminal loeschen + Cursor home (TeraTerm/ANSI)
    Serial.println("=== I2C SCAN MODE ===");
    Serial.println("--- Wire2 (Sensor/Storage, i2c1 GP2/GP3): MCP9808 0x18-1F, FRAM 0x50, DS3231 0x68, VEML7700 0x10 ---");
    i2cScan(Wire2, Serial);
    Serial.println("--- Wire  (DAC-Bus, i2c0 GP4/GP5): MCP4725 ---");
    i2cScan(Wire, Serial);
    Serial.println("(I2C_SCAN_MODE nach der Diagnose wieder auf 0 setzen)");
    delay(1500);
  }
#endif

  storage_load();  // FRAM wird direkt ueber Wire2 gelesen (kein fram.begin noetig)
  //storage_save();

#if RESET_ODO_ONCE
  // Muss NACH storage_load() stehen: dort werden Cold-Block und Hot-Datensatz geladen,
  // die sonst den genullten Wert sofort wieder ueberschreiben wuerden.
  p.odoKm  = 0.0f;
  p.tripKm = 0.0f;
  storage_save();   // schreibt Settings UND Hot-Datensatz (neue, hoehere Sequenznummer)
  Serial.println("ODO/TRIP GELOESCHT - RESET_ODO_ONCE wieder auf 0 setzen!");
#endif
  if (p.streetOnRestart) p.pwRace = 0;
  veml7700_begin();
  mcp9808_begin();
  ds3231_begin();
  vesc_begin();
  vesc_imu_calibrate();
  racekey_begin();

#if ENABLE_STATUS_LED
  ws2812_begin(LED_BOARD);
  mutex_enter_blocking(&led_mutex);
  ws2812_set(255, 0, 0); // Rot beim Start
  mutex_exit(&led_mutex);
  Serial.println("WS2812B_LED_setup");
#endif

  touch_button_setup();
  Serial.println("touch_button_setup");

  /* UI LVGL setup*/
  ui_setup();
  Serial.println("ui_setup");
  Serial.println("Display Init");

  /* UI */
  ui_init();

  lastButtonActivityMs = millis();
}

void loop() {

  lv_timer_handler();
  touch_button_handler();
  menu_handler();
  stopwatch_update();
  storage_handler();
  lights_handler();
  mcp9808_update();
  ds3231_update();
  vesc_update();
  calc_update();
  racekey_update();
  control_update();
  ui_update();
  //wsLed_toggle_handler();
#if !I2C_SCAN_MODE
  serial_update();  // im Scan-Modus AUS, damit das Dashboard die Scan-Ausgabe nicht ueberschreibt
#endif

  //delay(5);
}

// ================================================================
// CORE 1: ECHTZEIT MOTORSTEUERUNG
// ================================================================
void setup1() {
#if I2C_SCAN_MODE
  // Im Scan-Modus scannt Core 0 BEIDE Busse nacheinander. Core 1 haelt hier SOFORT still
  // (noch vor jeder Bus-Initialisierung), damit nur Core 0 Master auf den Bussen ist ->
  // keine Kollision, saubere Ausgabe. Motorsteuerung bleibt dabei aus.
  while (1) delay(1000);
#endif

  Wire.setSDA(SDA_PIN);
  Wire.setSCL(SCL_PIN);
  Wire.begin();
  Wire.setClock(100000);  // 100kHz: BSS138-Levelshifter ist bei 400kHz zu langsam
                          // (Anstiegszeit High-Side) -> Busfehler (endTransmission=4)

  // MCP4725: EEPROM auf 0 setzen falls nötig (Sicherheit: kein Vollgas bei Kommunikationsverlust)
  Wire.requestFrom(DAC_ADDR, (uint8_t)5);
  uint8_t b[5] = {};
  for (uint8_t i = 0; i < 5 && Wire.available(); i++) b[i] = Wire.read();
  if (((b[3] & 0x0F) != 0) || (b[4] != 0)) {
    Wire.beginTransmission(DAC_ADDR);
    Wire.write(0x60);
    Wire.write(0x00);
    Wire.write(0x00);
    Wire.endTransmission();
    delay(25);
  }
  // General Call Reset: DAC lädt EEPROM (=0) bei stabiler Spannung
  Wire.beginTransmission(0x00);
  Wire.write(0x06);
  Wire.endTransmission();
  delay(1);

  pinMode(TORQUE_PIN, INPUT);
  analogReadResolution(12);

  pinMode(CADENCE_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CADENCE_PIN), cadenceInterrupt, FALLING);

  pinMode(SPEED_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SPEED_PIN), speedInterrupt, FALLING);

  pinMode(CRUISE_PIN, INPUT_PULLUP);

  lastSpeedMs = millis(); // Sensor-Timeout startet erst ab jetzt

  delay(500);

  // Watchdog scharf schalten: ab jetzt muss loop1 (Core 1) regelmaessig fuettern.
  // Haengt die Echtzeit-Steuerung > WDT_TIMEOUT_MS, folgt ein Chip-Reset -> setup1
  // setzt den DAC per General-Call-Reset auf 0 V -> Motor aus. Nur Core 1 fuettert,
  // damit ein blockiertes Core 0 (LVGL/Display) NICHT die Steuerung resettet.
#if ENABLE_WATCHDOG
  rp2040.wdt_begin(WDT_TIMEOUT_MS);
#endif
}

void loop1() {
  // Heartbeat-LED nur 1 Hz togglen (rein kosmetisch, kein Grund fuer 100 Hz).
  // Der PIO-Treiber schiebt nur ein 24-Bit-Wort in die FIFO -> kein Interrupt-Sperren.
#if ENABLE_STATUS_LED
  static uint32_t lastLedMs = 0;
  if (millis() - lastLedMs >= 500) { wsLed_toggle_handler(); lastLedMs = millis(); }
#endif

  uint32_t loopStart = millis();

#if ENABLE_WATCHDOG
  rp2040.wdt_reset(); // Watchdog fuettern: solange loop1 laeuft, kein Reset
#endif

  // 0. GESCHWINDIGKEIT TIMEOUT (kein Puls -> Stillstand)
  // Letzten Messwert halten, bis der Timeout abgelaufen ist -> dann Stillstand.
  if ((millis() - lastSpeedMs) > SPEED_TIMEOUT_MS) {
    s.speed = 0.0f;
  }

  // Aktive Limits (Race / Street)
  float    speedLimit  = p.pwRace ? p.speedLimitRace  : p.speedLimitStreet;
  uint16_t powerLimit  = p.pwRace ? p.powerLimitRace  : p.powerLimitStreet;

  // Cruise-Limit: In den PEDAL-Modi (0/1/2) ist die Cruise-Taste eine Schiebehilfe und
  // bleibt IMMER auf cruiseLimitStreet begrenzt - auch wenn pwRace aktiv ist. Nur im
  // reinen Cruise-Modus (pasMode 3) greift die Race-Umschaltung.
  float    cruiseLimit = (p.pwRace && p.pasMode == 3) ? p.cruiseLimitRace
                                                      : p.cruiseLimitStreet;

  // Cruise-Taste: wirkt in ALLEN Modi und UEBERSTEUERT die PAS-Logik. Begrenzt bleibt
  // sie durch cruiseLimit (Geschwindigkeit) und cruisePower (Hoehe).
  bool cruisePressed = (digitalRead(CRUISE_PIN) == LOW);
  s.cruise = cruisePressed ? 1 : 0;

  // 1. KADENZ LOGIK (immer, für Displayanzeige)
  if ((millis() - lastPulseMs) > p.cadenceTimeoutMs) {
    filteredCadence = 0;
  } else {
    float safeRpm = (instantRpm > p.cadenceMaxLimit) ? filteredCadence : instantRpm;
    filteredCadence = (p.cadenceFilterAlpha * safeRpm) +
                      ((1.0f - p.cadenceFilterAlpha) * filteredCadence);
  }
  s.cadenceRpm = filteredCadence;
  s.cadence = constrain(filteredCadence / p.cadenceMaxRpm, 0.0f, 1.0f);

  // Kadenz-GATE (nur pasMode 0) - bewusst getrennt von cadenceTimeoutMs oben.
  // Reisst die Pulsfolge laenger als cadenceGateMs ab -> Zaehler zurueck -> Gate zu.
  // Sonst: offen, sobald cadenceGatePulses aufeinanderfolgende Pulse da sind.
  // Kennt KEINE Mindest-Drehzahl -> funktioniert auch beim zaehen Berg-Anfahren.
  if ((millis() - lastPulseMs) > p.cadenceGateMs) cadencePulseCount = 0;
  bool cadenceGateOpen = (cadencePulseCount >= p.cadenceGatePulses);

  // 2. DREHMOMENT LOGIK (immer, für Displayanzeige)
  // Sensor ist invertiert: hohe Spannung = kein Tritt, niedrige Spannung = Kraft
  // MEDIAN aus 3 Messungen statt des Rohwerts. Die Sensorleitung faengt sich
  // einzelne Spikes ein - im Log vom 03.08. sowohl nach unten (Rohwert bis 0 =
  // scheinbar VOLLE Kraft) als auch nach oben (4095 = Anschlag). Ein Median wirft
  // genau solche Einzelausreisser komplett weg, ohne die Reaktion zu verzoegern
  // wie ein Tiefpass (Verzug nur 1 Abtastung = 10 ms bei 100 Hz).
  // Ohne das rastet torqueFilterRise (0.4) jeden Spike ein: EIN Ausreisser hebt
  // filteredTorque um 40 % der Luecke an - beim Anfahren sprang die Unterstuetzung
  // dadurch auf 80 %, obwohl real nur ~30 % Pedalkraft anlag.
  static uint16_t tqBuf[3] = {0, 0, 0};
  static uint8_t  tqIdx = 0;
  tqBuf[tqIdx] = (uint16_t)analogReadShared(TORQUE_PIN);
  tqIdx = (uint8_t)((tqIdx + 1) % 3);
  {
    uint16_t x = tqBuf[0], y = tqBuf[1], z = tqBuf[2];
    s.torque = (x > y) ? ((y > z) ? y : ((x > z) ? z : x))
                       : ((x > z) ? x : ((y > z) ? z : y));
  }
  float rawNorm = 0.0f;
  int32_t delta = (int32_t)p.torqueZero - (int32_t)s.torque;
  if (delta > (int32_t)p.torqueDeadband) {
    // Skalierung ab der Deadband-Schwelle, damit rawNorm stetig bei 0 startet
    // (kein Sprung) und erst bei torqueMax 1.0 erreicht.
    int32_t range = (int32_t)p.torqueZero - (int32_t)p.torqueMax - (int32_t)p.torqueDeadband;
    if (range > 0)
      rawNorm = constrain((float)(delta - (int32_t)p.torqueDeadband) / (float)range, 0.0f, 1.0f);
  }
  // ANZEIGE-Filter: laeuft IMMER, unabhaengig vom Gate. Nur so sieht man am Display
  // und im CSV, was der Sensor liefert - auch bei stehendem Pedal.
  float alpha = (rawNorm > filteredTorque) ? p.torqueFilterRise : p.torqueFilterFall;
  filteredTorque = (alpha * rawNorm) + ((1.0f - alpha) * filteredTorque);

  // REGEL-Filter: gleiche Parameter, aber bei geschlossenem Gate hart auf 0 gehalten.
  // Ohne das laedt er sich waehrend des Aufsteigens voll, WEIL der Fahrer schon aufs
  // Pedal drueckt bevor sich die Kurbel dreht - das Gate blockiert dann nur den
  // Ausgang, nicht den Filter. Beim ersten Kadenzpuls springt target damit in EINEM
  // Zyklus auf den Endwert statt aufzubauen.
  // Log 04.08. 21:01, Anfahren aus dem Stand: t=2066 filteredTorque 0.938 bei
  // target 0.000 -> t=2087 Gate auf -> target 0.930 sofort -> 0.65 s spaeter
  // 26.5 A Motorstrom bei Speed 0.0 = voller Schlag in den Antriebsstrang.
  static float gatedTorque = 0.0f;
  if (!cadenceGateOpen) {
    gatedTorque = 0.0f;
  } else {
    float gAlpha = (rawNorm > gatedTorque) ? p.torqueFilterRise : p.torqueFilterFall;
    gatedTorque = (gAlpha * rawNorm) + ((1.0f - gAlpha) * gatedTorque);
  }

  // Pedalerkennung ueber das ROHE Drehmoment (rawNorm > 0 = Druck ueber Deadband).
  // Bewusst nicht filteredTorque: dessen Abfall ist absichtlich traege (torqueFilterFall)
  // und wuerde den Nachlauf nur verlagern.
  static uint32_t lastTorqueMs = 0;
  if (rawNorm > 0.0f) lastTorqueMs = millis();
  bool pedaling = (millis() - lastTorqueMs) < p.torqueIdleMs;

  // 3. ZIEL-THROTTLE berechnen
  //
  // 3a) CRUISE-INTEGRATOR - uebersteuert in JEDEM Modus.
  // Der Throttle ist hier ein INTEGRATOR, kein Sollwert:
  //   unter allen Limits (mit Totzone) -> langsam hoch  (cruiseRampUp)
  //   ueber einem Limit                -> zurueck       (cruiseRampDown)
  //   dazwischen                       -> HALTEN        -> pendelt sich ein
  // Waere target einfach immer cruisePower, wuerde der Throttle beim Loslassen eines
  // Limits sofort wieder hochdruecken => staendiges Pendeln statt konstantem Fahren.
  // cruiseDeadband ist ein ANTEIL von cruiseLimit (0.10 = 10%), kein km/h-Wert -> die
  // Totzone skaliert mit dem Limit, und da der Faktor auf 0.5 begrenzt ist, kann
  // (cruiseLimit - deadband) nie <= 0 werden -> der Throttle rampt immer hoch.
  static float    cruiseHold   = 0.0f;
  static uint32_t lastCorrMs   = 0;
  if (cruisePressed) {
    float deadband   = cruiseLimit * constrain((float)p.cruiseDeadband, 0.0f, 0.5f);
    float ceiling    = (float)p.cruisePower / 100.0f;
    bool  overPower  = (s.power   > (float)powerLimit);
    bool  overCurrent= (s.current > p.currentMax);

    if (s.speed <= 0.0f && !overPower && !overCurrent) {
      // Anfahren aus dem Stand: Es gibt noch keinen gueltigen Speed-Messwert (unter
      // v_min liefert der Sensor nichts). Hier wird kontinuierlich mit 100 Hz Gas
      // aufgebaut, sonst kaeme das Bike gar nicht erst in Bewegung.
      if (cruiseHold < ceiling) cruiseHold += p.cruiseRampUp;

    } else if ((millis() - lastCorrMs) >= CRUISE_CORRECT_MS) {
      // Sobald eine Geschwindigkeit messbar ist: nur noch GETAKTET korrigieren.
      // Sonst reagiert der 100-Hz-Loop bis zu 138x auf denselben veralteten Messwert.
      lastCorrMs = millis();
      bool over  = (s.speed > cruiseLimit) || overPower || overCurrent;
      bool under = (s.speed < (cruiseLimit - deadband)) && !overPower && !overCurrent;

      if      (over)                          cruiseHold -= p.cruiseRampDown;
      else if (under && cruiseHold < ceiling) cruiseHold += p.cruiseRampUp;
      // sonst: halten -> pendelt sich auf dem Sollwert ein
    }
    cruiseHold = constrain(cruiseHold, 0.0f, ceiling);

  } else {
    lastCorrMs = 0;
    // Taste LOSGELASSEN -> Cruise sofort inaktiv. Der Throttle faellt danach mit der
    // normalen (schnellen) rampDown ab, NICHT mit cruiseRampDown.
    // Wichtig: cruiseRampDown ist ausschliesslich der REGEL-Parameter fuer "zu schnell,
    // Gas langsam zurueck" waehrend die Taste gedrueckt ist - nicht fuers Loslassen.
    cruiseHold = 0.0f;
  }

  // 3b) PAS-ZIEL (Pedal-Modi 0/1/2). pasMode 3 = reiner Cruise-Modus -> bleibt 0.
  float pasTarget = 0.0f;
  if (p.pasMode == 0) {
    // Drehmoment bestimmt die HOEHE, die Kadenz ist nur ein Ein/Aus-Gate.
    // gatedTorque statt filteredTorque: nur der wird bei geschlossenem Gate auf 0
    // gehalten und baut danach ueber torqueFilterRise auf, statt zu springen.
    // Die Gate-Abfrage ist dadurch doppelt gemoppelt - bewusst stehen gelassen,
    // damit die Sperre auch dann haelt, wenn der Filterblock spaeter umgebaut wird.
    float factor = cadenceGateOpen ? gatedTorque : 0.0f;
    float curveOut = applyCurve(factor);
    if (curveOut > 0.0f) curveOut = constrain(curveOut + p.curveOffset, 0.0f, 1.0f);
    pasTarget = curveOut * ((float)p.supportLevel / 12.0f);

  } else if (p.pasMode == 1) {
    // Drehmoment x Kadenz -> beide bestimmen gemeinsam die Hoehe (multiplikativ)
    float factor = filteredTorque * s.cadence;
    float curveOut = applyCurve(factor);
    if (curveOut > 0.0f) curveOut = constrain(curveOut + p.curveOffset, 0.0f, 1.0f);
    pasTarget = curveOut * ((float)p.supportLevel / 12.0f);

  } else if (p.pasMode == 2) {
    // Nur die Kadenz bestimmt die Hoehe (Drehmoment geht nicht in den Pegel ein -
    // wirkt aber weiterhin als Ein/Aus-Kriterium, siehe `pedaling` unten)
    float curveOut = applyCurve(s.cadence);
    if (curveOut > 0.0f) curveOut = constrain(curveOut + p.curveOffset, 0.0f, 1.0f);
    pasTarget = curveOut * ((float)p.supportLevel / 12.0f);
  }

  // Das DREHMOMENT ist in allen Pedal-Modi (0/1/2) das Haupt-Ein/Aus-Kriterium:
  // kein Pedaldruck laenger als torqueIdleMs -> kein PAS-Antrieb. Bewusst ueber das
  // Drehmoment und nicht ueber die Kadenz, weil der PAS (DM02) mit dem Antrieb
  // mitdreht und weiterzaehlt, auch wenn nicht getreten wird.
  if (!pedaling) pasTarget = 0.0f;

  // 3c) CRUISE UEBERSTEUERT: der groessere von beiden gewinnt. cruiseActive gilt NUR bei
  // gedrueckter Taste -> nur dann werden die (langsamen) Cruise-Rampen benutzt. Beim
  // Loslassen faellt cruiseHold auf 0 und der Throttle geht mit rampDown schnell runter.
  bool  cruiseActive = cruisePressed && (cruiseHold >= pasTarget);
  float target       = (cruiseHold > pasTarget) ? cruiseHold : pasTarget;

  // 4. GEMEINSAME LIMITS
  // Speed: gleitendes Ausblenden statt hartem Cutoff. Ab (speedLimit - SPEED_TAPER_KMH)
  // wird linear auf 0 heruntergefahren, bei speedLimit ist die Unterstuetzung exakt 0.
  // Ein harter Sprung auf 0 laesst das Ziel zwischen vollem PAS-Wert und 0 pendeln
  // -> spuerbares "Pumpen" an der Limit-Grenze.
  if (s.speed > speedLimit - SPEED_TAPER_KMH) {
    float f = (speedLimit - s.speed) / SPEED_TAPER_KMH;
    target *= constrain(f, 0.0f, 1.0f);
  }

  // Speed-Sensor Ausfall: kein Puls seit 4s trotz aktivem Tritt → Motor sperren
  // Race-Ausnahme: speedLimitRace = 150 km/h (Maximum) → kein Sensor nötig
  // Cruise-Ausnahme: Im Schiebetempo liegen die Speed-Pulse weit auseinander, und der
  // PAS dreht mit dem Antrieb mit (filteredCadence > 0) -> die Sperre wuerde beim
  // Anfahren mit Cruise faelschlich zuschlagen. Cruise ist ohnehin durch cruiseLimit
  // und cruisePower begrenzt.
  bool speedSensorLost = ((millis() - lastSpeedMs) > 4000) && (filteredCadence > 0.05f);
  if (speedSensorLost && !cruisePressed && (!p.pwRace || p.speedLimitRace < 150.0f)) target = 0.0f;

  // VESC-Daten Ausfall: keine gültige Antwort seit 2s → Strom/Leistung unbekannt → Motor sperren
  // Race-Ausnahme: powerLimitRace = 5000 W (Maximum) → kein Power-Limit nötig
  bool vescDataLost = (millis() - lastVescValuesMs) > 4000;
  if (vescDataLost && (!p.pwRace || p.powerLimitRace < 5000)) target = 0.0f;

  // Power & Current: proportionale Reduktion -> stabilisiert sich am Limit,
  // kein Flattern wie bei hartem Cutoff (VESC-Update-Latenz 200ms).
  // Snapshot der volatile-Werte (von Core 0 geschrieben), damit Check und Division
  // denselben Wert nutzen -> kein Divide-by-zero-Fenster.
  // Uebersprungen, solange Cruise dominiert: dort regelt der Integrator Power/Strom
  // bereits selbst, eine zusaetzliche proportionale Ruecknahme wuerde doppelt wirken.
  float curPower   = s.power;
  float curCurrent = s.current;
  if (!cruiseActive) {
    if (curPower > (float)powerLimit && curPower > 0.0f)
      target *= ((float)powerLimit / curPower);
    if (curCurrent > p.currentMax && curCurrent > 0.0f)
      target *= (p.currentMax / curCurrent);
  }

  // Harte Sperren oben (Speed-Limit, Sensor-/VESC-Ausfall) muessen auch den Cruise-
  // Integrator zuruecknehmen, sonst zieht er den Throttle sofort wieder hoch.
  if (target < cruiseHold) cruiseHold = target;

  // Diagnose speichern (für CSV-Logging)
  s.diagRawNorm = rawNorm;
  s.diagFilteredTorque = filteredTorque;
  s.diagGatedTorque = gatedTorque;
  s.diagTarget = target;
  s.diagCadenceGateOpen = cadenceGateOpen ? 1 : 0;

  // 5. RAMPE - Cruise nutzt seine eigenen (sanfteren) Rampen. `cruiseActive` bleibt beim
  // Loslassen so lange wahr, wie cruiseHold noch ausklingt -> die Cruise-Ramp-Down greift
  // also auch dann, obwohl cruisePressed schon false ist.
  // Die Rampenwahl haengt am IST-Throttle, nicht am Ziel: der erste Teil des Gaswegs
  // (bis rampThreshold) laeuft dadurch IMMER mit der langsamen rampUpLow, egal wie hoch
  // das Ziel springt. Mit `target > rampThreshold` war es genau umgekehrt - ein hohes
  // Ziel schaltete sofort auf rampUpHigh und rampUpLow kam beim Anfahren nie zum Zug
  // (Log 04.08. 21:01: target sprang auf 0.93 -> Vollgas in 0.65 s aus dem Stand).
  if (target > throttle) {
    float step = cruiseActive ? p.cruiseRampUp
                              : ((throttle > p.rampThreshold) ? p.rampUpHigh : p.rampUpLow);
    throttle += step;
  } else if (throttle > target) {
    throttle -= cruiseActive ? p.cruiseRampDown : p.rampDown;
  }

  // Sofortstopp: Will WEDER Cruise NOCH das PAS Antrieb, faellt der Throttle sofort auf 0.
  // Der PAS-Anteil ist oben bereits ueber `pedaling` (Drehmoment-Timeout) gesperrt, der
  // Cruise-Anteil klingt ueber cruiseHold gleitend aus -> waehrend des Ausklingens ist
  // target > 0 und dieser Cutoff greift bewusst NICHT (sonst gaebe es einen Ruck).
  if (target <= 0.0f) {
    throttle = 0.0f;
  }

  throttle = constrain(throttle, 0.0f, 1.0f);

  // throttle 0..1 -> 0..THROTTLE_MAX_V am VESC-ADC (nie ueber 3.3V)
  uint16_t dacVal = (uint16_t)(throttle * (THROTTLE_MAX_V / DAC_VREF_V) * 4095.0f);
  sendToDAC(dacVal);
  s.throttleVolt = (float)dacVal * (DAC_VREF_V / 4095.0f); // tatsaechliche Ausgangsspannung

  // Stopwatch wird auf Core 0 (stopwatch_update) ueber millis()-Delta gefuehrt,
  // damit kein Cross-Core-RMW auf p.stopwatchTicks entsteht.

  // 100Hz Taktung
  while (millis() - loopStart < 10) {
    yield();
  }
}


inline void fastToggle(uint pin) {
    sio_hw->gpio_togl = 1u << pin;
}

void wsLed_toggle_handler(){
  mutex_enter_blocking(&led_mutex);
  if (wsLed_state) {
    ws2812_set(50, 0, 0);  // gedimmtes Rot
  } else {
    ws2812_set(0, 0, 0);   // aus
  }
  mutex_exit(&led_mutex);

  wsLed_state = !wsLed_state;
}

void lv_log_cb(const char * buf) {
    if (Serial) {
        Serial.print(buf);
    }
}
