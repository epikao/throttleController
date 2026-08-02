#include <Arduino.h>
#include "globals.h"
#include "mcp9808.h"

#define MCP9808_REG_CONF     0x01
#define MCP9808_REG_TEMP     0x05
#define MCP9808_REG_MANUF_ID 0x06   // liefert konstant 0x0054 -> zur Identifikation
#define MCP9808_REG_RES      0x08
#define MCP9808_MANUF_ID     0x0054

// Aktive Sensor-Adresse (0 = noch nicht gefunden). Der MCP9808 kann per A0-A2 irgendwo
// auf 0x18-0x1F liegen; deshalb wird sie nicht hart verdrahtet, sondern beim Start
// gesucht und ueber die Hersteller-ID bestaetigt. So ist der Temperaturwert immun gegen
// verschobene Adress-Pins / getauschte Module.
static uint8_t s_addr = 0;

static bool mcp9808_readReg16(uint8_t addr, uint8_t reg, uint16_t& out) {
    Wire2.beginTransmission(addr);
    Wire2.write(reg);
    if (Wire2.endTransmission() != 0) return false;          // kein ACK -> nichts da
    if (Wire2.requestFrom(addr, (uint8_t)2) < 2) return false;
    out  = (uint16_t)Wire2.read() << 8;
    out |= Wire2.read();
    return true;
}

// Sucht den MCP9808 auf 0x18-0x1F und verifiziert ihn ueber die Hersteller-ID.
// Setzt s_addr bei Erfolg und konfiguriert den Sensor (Normal-Mode, 12-bit).
static void mcp9808_detect() {
    s_addr = 0;
    for (uint8_t a = 0x18; a <= 0x1F; a++) {
        uint16_t id;
        if (mcp9808_readReg16(a, MCP9808_REG_MANUF_ID, id) && id == MCP9808_MANUF_ID) {
            s_addr = a;
            break;
        }
    }
    if (s_addr == 0) return;

    // Config: normal mode (kein Shutdown)
    Wire2.beginTransmission(s_addr);
    Wire2.write(MCP9808_REG_CONF);
    Wire2.write(0x00);
    Wire2.write(0x00);
    Wire2.endTransmission();

    // Resolution explizit auf 12-bit setzen (0.0625°C, 240ms Wandlungszeit)
    Wire2.beginTransmission(s_addr);
    Wire2.write(MCP9808_REG_RES);
    Wire2.write(0x03);
    Wire2.endTransmission();
}

void mcp9808_begin() {
    mcp9808_detect();
}

void mcp9808_update() {
    static uint32_t lastMs = 0;
    if (millis() - lastMs < 1000) return;
    lastMs = millis();

    // Sensor (noch) nicht gefunden -> einmal pro Sekunde erneut suchen (self-healing,
    // falls der Bus beim Boot noch nicht bereit war oder der Sensor spaeter dazukommt).
    if (s_addr == 0) {
        mcp9808_detect();
        if (s_addr == 0) return;
    }

    Wire2.beginTransmission(s_addr);
    Wire2.write(MCP9808_REG_TEMP);
    Wire2.endTransmission(); // normaler Stop, kein repeated start

    if (Wire2.requestFrom(s_addr, (uint8_t)2) < 2) { s_addr = 0; return; } // weg -> neu suchen

    uint16_t raw = (uint16_t)Wire2.read() << 8;
    raw |= Wire2.read();
    raw &= 0x1FFF; // Alert-Flags (Bits 15:13) entfernen

    float t;
    if (raw & 0x1000) {
        t = (float)(raw & 0x0FFF) * 0.0625f - 256.0f; // negativ
    } else {
        t = (float)(raw & 0x0FFF) * 0.0625f;           // positiv
    }

    if (t > -40.0f && t < 125.0f) s.temp = t + p.tempCalOffset;
}
