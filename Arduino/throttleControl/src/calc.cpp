#include <Arduino.h>
#include "globals.h"

// ---------------------------------------------------------------------------
// Quelle für s.voltage und s.current:
//   0 = extern (UART / Bus) — Werte werden von aussen gesetzt
//   1 = ADC
// ---------------------------------------------------------------------------
#define VOLT_CURR_SOURCE 0

#if VOLT_CURR_SOURCE == 1
// ADC-Kalibrierung: an Hardware anpassen
// Spannung: VOLT_ADC_GAIN = (R1 + R2) / R2 des Spannungsteilers
//   Beispiel R1=100kΩ, R2=5.6kΩ → 18.86, messbar bis ~62V
#define VOLT_ADC_GAIN    18.86f
// Strom: Beispiel ACS712-20A (100mV/A, 0A = Mitte des ADC-Bereichs)
//   CURR_ADC_OFFSET = ADC-Rohwert bei 0A
//   CURR_ADC_GAIN   = A pro ADC-Count = 3.0 / (4095 × Sensitivität_V/A)
#define CURR_ADC_OFFSET  2048
#define CURR_ADC_GAIN    0.008f
#endif

// ---------------------------------------------------------------------------
// SOC aus Spannung: Li-Ion NMC Entladekurve (OCV, normiert)
// Nur einmalig beim Start verwendet — Akku idealerweise im Ruhezustand
// ---------------------------------------------------------------------------
static uint8_t soc_from_voltage() {
    if (p.voltageMax <= p.voltageMin || s.voltage <= 0.0f) return 50;
    static const struct { float x; uint8_t soc; } tbl[] = {
        {0.00f,  0}, {0.10f,  3}, {0.20f,  9}, {0.30f, 18},
        {0.40f, 30}, {0.50f, 50}, {0.60f, 65}, {0.70f, 78},
        {0.80f, 88}, {0.90f, 95}, {1.00f, 100},
    };
    constexpr uint8_t n = sizeof(tbl) / sizeof(tbl[0]);
    float x = constrain((s.voltage - p.voltageMin) / (p.voltageMax - p.voltageMin), 0.0f, 1.0f);
    if (x >= tbl[n-1].x) return tbl[n-1].soc;
    for (uint8_t i = 1; i < n; i++) {
        if (x <= tbl[i].x) {
            float t = (x - tbl[i-1].x) / (tbl[i].x - tbl[i-1].x);
            return (uint8_t)(tbl[i-1].soc + t * (tbl[i].soc - tbl[i-1].soc));
        }
    }
    return tbl[n-1].soc;
}

// ---------------------------------------------------------------------------

static float tripWhAccum = 0.0f;
static float sessionWh   = 0.0f;
static float initialWh   = -1.0f;
static float saveAccum   = 0.0f;  // km-Zaehler NUR als periodischer Speicher-/Wh-Trigger
                                  // (odoKm/tripKm zaehlen laufend, unabhaengig davon).
static float sessionKm   = 0.0f;  // Strecke seit Boot/Trip-Reset, gepaart mit tripTimeH/tripWhAccum

void calc_trip_reset() {
    tripWhAccum = 0.0f;
    sessionKm   = 0.0f;
    s.whKm = 0.0f;
    s.tripTimeH = 0.0f;
    s.speedAvg = 0.0f;
}

void calc_update() {
    static uint32_t lastMs = 0;
    uint32_t now = millis();
    if (now - lastMs < 100) return;
    float dtH = (now - lastMs) / 3600000.0f;
    lastMs = now;

#if VOLT_CURR_SOURCE == 1
    s.voltage = (float)analogReadShared(VOLTAGE_PIN) / 4095.0f * 3.0f * VOLT_ADC_GAIN;
    float rawCurr = (float)analogReadShared(CURRENT_PIN) - CURR_ADC_OFFSET;
    s.current = constrain(rawCurr * CURR_ADC_GAIN, 0.0f, 999.0f);
#endif

    // --- Leistung [W] ---
    s.power = (uint16_t)(s.voltage * s.current);

    // --- Initialisierung: SOC einmalig aus OCV schätzen ---
    if (initialWh < 0.0f && s.voltage > 1.0f && p.capacityMax > 0.0f)
        initialWh = (soc_from_voltage() / 100.0f) * p.capacityMax;

    // --- Distanz & Energie (nur bei Fahrt) ---
    if (s.speed > 0.5f) {
        float dKm = s.speed * dtH;
        float dWh = (float)s.power * dtH;
        s.tripTimeH += dtH;

        p.tripKm    += dKm;
        p.odoKm     += dKm;   // Gesamt-km laufend mitzaehlen (float -> exakt). odoKm und
                              // tripKm zaehlen identisch hoch und werden gemeinsam im Struct
                              // gesichert -> odo = alter Stand + tripKm bleibt immer erfuellt,
                              // auch ueber Aus-/Einschalten hinweg.
        sessionKm   += dKm;
        tripWhAccum += dWh;
        sessionWh   += dWh;
        saveAccum   += dKm;

        // Jeden vollen km: nur den Hot-Datensatz (Trip/Odo/Wh-pro-km) sichern - das ist
        // EINE Page statt des kompletten Settings-Blocks (bei EEPROM 1 statt 6 Zyklen).
        if (saveAccum >= 1.0f) {
            saveAccum -= (float)(uint32_t)saveAccum;
            if (sessionKm > 2.0f)     // erst nach 2 km: Wert ist repräsentativ
                p.lastWhKm = s.whKm;
            storageHotDirty = true;
        }
    }

    // --- Wh/km & Durchschnittsgeschwindigkeit seit letztem Trip-Reset ---
    // sessionKm (statt persistentem tripKm) -> Zaehler & Nenner aus demselben
    // Zeitfenster, sonst Blowup nach Neustart (tripTimeH startet bei 0)
    if (sessionKm > 0.1f) {
        s.whKm = tripWhAccum / sessionKm;
        if (s.tripTimeH > 0.0f)
            // Anzeige kann nur 2 Vorkommastellen -> auf 99.9 km/h begrenzen
            s.speedAvg = constrain(sessionKm / s.tripTimeH, 0.0f, 99.9f);
    }

    // --- Kapazität & Restreichweite aus Energy-Accounting ---
    if (initialWh >= 0.0f && p.capacityMax > 0.0f) {
        float remainingWh = max(0.0f, initialWh - sessionWh);
        s.capacity = (uint8_t)constrain(remainingWh / p.capacityMax * 100.0f, 0.0f, 100.0f);
        float effectiveWhKm = (s.whKm > 0.5f) ? s.whKm : p.lastWhKm;
        if (effectiveWhKm > 0.5f)
            s.range = (uint16_t)constrain(remainingWh / effectiveWhKm, 0.0f, 9999.0f);
    }
}
