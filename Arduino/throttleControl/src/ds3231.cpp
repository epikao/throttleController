#include <Arduino.h>
#include "globals.h"
#include "menu.h"

#define DS3231_ADDR 0x68

static uint8_t bcd2dec(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }
static uint8_t dec2bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

void ds3231_begin() {
    // 24h-Modus sicherstellen: Bit 6 in Stunden-Register (0x02) = 0
    Wire2.beginTransmission(DS3231_ADDR);
    Wire2.write(0x02);
    Wire2.endTransmission(false);
    if (Wire2.requestFrom((uint8_t)DS3231_ADDR, (uint8_t)1) < 1) return;
    uint8_t hrReg = Wire2.read();
    if (hrReg & 0x40) {
        // 12h-Modus → Stunde extrahieren und in 24h neu schreiben
        uint8_t h = bcd2dec(hrReg & 0x1F);
        if (hrReg & 0x20) h += 12; // PM
        Wire2.beginTransmission(DS3231_ADDR);
        Wire2.write(0x02);
        Wire2.write(dec2bcd(h % 24) & 0x3F);
        Wire2.endTransmission();
    }
}

void ds3231_update() {
    if (menuState >= 61 && menuState < 70) return; // nicht während Zeiteinstellung überschreiben
    static uint32_t lastMs = 0;
    if (millis() - lastMs < 1000) return;
    lastMs = millis();

    Wire2.beginTransmission(DS3231_ADDR);
    Wire2.write(0x00); // Start bei Sekunden
    Wire2.endTransmission(false);
    if (Wire2.requestFrom((uint8_t)DS3231_ADDR, (uint8_t)3) < 3) return;

    uint8_t sec  = bcd2dec(Wire2.read() & 0x7F);
    uint8_t min  = bcd2dec(Wire2.read() & 0x7F);
    uint8_t hour = bcd2dec(Wire2.read() & 0x3F);

    s.time   = (uint32_t)hour * 3600 + (uint32_t)min * 60 + sec;
    s.onTime = millis() / 1000;
}

void ds3231_set(uint32_t t) {
    Wire2.beginTransmission(DS3231_ADDR);
    Wire2.write(0x00);
    Wire2.write(dec2bcd(t % 60));               // Sekunden
    Wire2.write(dec2bcd((t / 60) % 60));        // Minuten
    Wire2.write(dec2bcd((t / 3600) % 24) & 0x3F); // Stunden, 24h-Modus
    Wire2.endTransmission();
}
