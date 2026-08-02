#include <Arduino.h>
#include "globals.h"


/* Storage (FRAM oder EEPROM) + I2C Bus */
TwoWire Wire2(i2c1, 2, 3); //SDA,SCL

/* Gemeinsamer ADC-Zugriff (Core 0 + Core 1) */
// auto_init_mutex: garantiert initialisiert vor main() -> keine Core-Reihenfolge-Annahme
auto_init_mutex(adc_mutex);

int analogReadShared(uint8_t pin) {
  mutex_enter_blocking(&adc_mutex);
  int v = analogRead(pin);
  mutex_exit(&adc_mutex);
  return v;
}

/* Temperature sensor */


/* RTC */
