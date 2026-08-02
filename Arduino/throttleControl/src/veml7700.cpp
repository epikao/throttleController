#include <Arduino.h>
#include "globals.h"

#define VEML7700_ADDR     0x10
#define VEML7700_REG_CONF 0x00
#define VEML7700_REG_ALS  0x04
#define VEML7700_LUX_RES  0.0576f   // gain=1x, IT=100ms

static void writeReg(uint8_t reg, uint16_t data) {
    Wire2.beginTransmission(VEML7700_ADDR);
    Wire2.write(reg);
    Wire2.write((uint8_t)(data & 0xFF));
    Wire2.write((uint8_t)(data >> 8));
    Wire2.endTransmission();
}

static uint16_t readReg(uint8_t reg) {
    Wire2.beginTransmission(VEML7700_ADDR);
    Wire2.write(reg);
    Wire2.endTransmission(false);
    Wire2.requestFrom((uint8_t)VEML7700_ADDR, (uint8_t)2);
    uint16_t val = (uint16_t)Wire2.read();
    val |= (uint16_t)Wire2.read() << 8;
    return val;
}

void veml7700_begin() {
    // Gain=1x (bits 12:11=00), IT=100ms (bits 9:6=0000), power on (bit0=0)
    writeReg(VEML7700_REG_CONF, 0x0000);
}

uint16_t veml7700_autobrightness() {
    static float filtered = -1.0f;
    static uint32_t lastMs = 0;

    if (millis() - lastMs > 200) {
        float lux = readReg(VEML7700_REG_ALS) * VEML7700_LUX_RES;
        if (filtered < 0.0f) {
            filtered = lux;
        } else {
            // schnell abdunkeln (alpha=0.5), langsam aufhellen (alpha=0.05)
            float alpha = (lux < filtered) ? 0.5f : 0.05f;
            filtered = alpha * lux + (1.0f - alpha) * filtered;
        }
        lastMs = millis();
    }

    float f = max(0.0f, filtered);
    float b;
    if (f < 100.0f) {
        // linear: 0 lux -> 1%, 100 lux -> 50%  (steiler Dunkelbereich)
        b = f / 100.0f * 49.0f + 1.0f;
    } else {
        // logarithmisch: 100 lux -> 50%, 10000 lux -> 100%  (unverändert)
        b = (log10f(f) / log10f(10000.0f)) * 99.0f + 1.0f;
    }
    return (uint16_t)constrain(b, 1.0f, 100.0f);
}
