#pragma once
#include <Wire.h>

// Scannt den I2C-Bus (Adressen 1..126) und gibt gefundene Geraete ueber `out` aus.
// Liefert die Anzahl gefundener Geraete zurueck. Adressen im Bereich 0x60..0x67
// werden als moeglicher MCP4725 markiert.
uint8_t i2cScan(TwoWire& bus, Print& out);
