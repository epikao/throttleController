#pragma once

void vesc_begin(void);
void vesc_update(void);
void vesc_imu_calibrate(void);  // Kalibrierung beim Start (Bike im Stillstand)

extern volatile uint32_t lastVescValuesMs; // Zeitstempel letzte gültige VESC-Antwort (Core 0 -> Core 1)
