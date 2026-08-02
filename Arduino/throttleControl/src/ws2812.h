#pragma once

#include <stdint.h>

// Minimaler WS2812/NeoPixel-Treiber fuer EINE LED auf dem RP2350 (PIO-basiert).
// Ersetzt die Adafruit_NeoPixel-Library -> keine externe Abhaengigkeit mehr.
// Verwendet pio0 / State-Machine 0. Kein Interrupt-Sperren (anders als Bit-Bang):
// die Bits werden von der PIO getaktet, die CPU schiebt nur ein 24-Bit-Wort in die FIFO.
void ws2812_begin(uint8_t pin);          // einmalig im setup() aufrufen
void ws2812_set(uint8_t r, uint8_t g, uint8_t b);  // Farbe setzen (nicht-blockierend)
