#include <Arduino.h>
#include "i2c_scanner.h"

static void printHex2(Print& out, uint8_t v) {
    out.print("0x");
    if (v < 16) out.print('0');
    out.print(v, HEX);
}

uint8_t i2cScan(TwoWire& bus, Print& out) {
    uint8_t count = 0;
    out.print("I2C scan: ");
    for (uint8_t addr = 1; addr < 127; addr++) {
        bus.beginTransmission(addr);
        // endTransmission(): 0 = Geraet hat geackt (vorhanden), sonst kein ACK
        if (bus.endTransmission() == 0) {
            printHex2(out, addr);
            if (addr >= 0x60 && addr <= 0x67) out.print("(MCP4725?)");
            out.print(' ');
            count++;
        }
    }
    if (count == 0) out.print("(nichts gefunden)");
    out.print(" -> ");
    out.print(count);
    out.println(" Geraet(e)");
    return count;
}
