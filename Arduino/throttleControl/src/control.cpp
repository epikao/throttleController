#include <Arduino.h>
#include "globals.h"
#include "storage.h"

#define CONTROL_ARM_MS      50    // Mindestdauer unter Hysteresespannung zum Armen
#define CONTROL_TRIG_MS     80    // Mindestdauer ueber Schwelle zum Ausloesen (Entstoerung
                                  // gegen EMV-Spikes, z.B. waehrend Motortest)
#define CONTROL_HYSTERESIS  0.1f  // [V] unterhalb Schwellwert zum Armen

// Zustände: Spannung zunächst hoch → ignorieren
//           Spannung unter (Schwelle - 0.1V) für CONTROL_ARM_MS → armen
//           Spannung über Schwelle für CONTROL_TRIG_MS (ununterbrochen) → auslösen
void control_update() {
    enum State { WAIT_LOW, ARMED, TRIGGERED };
    static State    state     = WAIT_LOW;
    static uint32_t lowSince  = 0;
    static uint32_t highSince = 0;

    if (state == TRIGGERED) return;

    float v = analogReadShared(CONTROL_IN_PIN) * (3.0f / 4095.0f);

    if (state == WAIT_LOW) {
        if (v < (p.controlThresholdV - CONTROL_HYSTERESIS)) {
            if (lowSince == 0) lowSince = millis();
            else if ((millis() - lowSince) >= CONTROL_ARM_MS) state = ARMED;
        } else {
            lowSince = 0;
        }
    } else {  // ARMED
        if (v > p.controlThresholdV) {
            // Nur ausloesen, wenn die Spannung ununterbrochen ueber der Schwelle
            // bleibt -> ein einzelner Stoerspike schaltet nicht mehr aus.
            if (highSince == 0) highSince = millis();
            else if ((millis() - highSince) >= CONTROL_TRIG_MS) {
                state = TRIGGERED;
                // NOCH VOR dem Kappen der Selbsthaltung sichern: die Versorgung steht hier
                // garantiert noch. Rettet den angefangenen Kilometer (Trip/Odo liegen sonst
                // nur im RAM) und noch nicht persistierte Parameteraenderungen.
                storage_save();
                digitalWrite(CONTROL_OUT_PIN, LOW);
            }
        } else {
            highSince = 0;  // kurzer Spike -> Timer zuruecksetzen, kein Ausloesen
        }
    }
}
