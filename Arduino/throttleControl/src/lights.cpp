#include <Arduino.h>
#include "globals.h"
#include "veml7700.h"

// Durchgaengig PWM (analogWrite), KEIN Wechsel zu digitalWrite: auf dem RP2040 wuerde
// das Umschalten SIO<->PWM den Pad-Function-Select bei jedem Aufruf umrouten (Glitches)
// und die von GPIO22/23 geteilte PWM-Slice stoeren. Extremwerte als reines DC:
//   duty 0   = Dauer-LOW  = voll an (active-LOW EN)
//   duty 255 = Dauer-HIGH = aus
static void applyLight(uint8_t pin, bool on, uint16_t dimPercent) {
    uint8_t duty;
    if (!on || dimPercent == 0) {
        duty = 255;                      // aus
    } else if (dimPercent >= 100) {
        duty = 0;                        // voll an
    } else {
        duty = 255 - (uint8_t)((uint32_t)dimPercent * 255 / 100);
    }
    analogWrite(pin, duty);
}

void lights_handler() {
    applyLight(FRONT_LIGHT_PIN, p.frontLightOnOff, p.frontLightDim);
    // Bremslicht: bei Bremserkennung immer 100%, sonst normale Dimmung
    if (s.braking && p.rearLightOnOff)
        analogWrite(REAR_LIGHT_PIN, 0);  // active-LOW: voll an (Dauer-LOW, kein digitalWrite-Wechsel)
    else
        applyLight(REAR_LIGHT_PIN, p.rearLightOnOff, p.rearLightDim);

    uint16_t blDim = (p.backlightDim == 101) ? veml7700_autobrightness() : p.backlightDim;
    bool blOn = (p.onTimeBacklight == 1000) ||
                ((millis() - lastButtonActivityMs) < ((uint32_t)p.onTimeBacklight * 1000));
    applyLight(BACKLIGHT_PIN, blOn, blDim);
}
