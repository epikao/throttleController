#pragma once

void ds3231_begin(void);
void ds3231_update(void);      // aktualisiert s.time (Sekunden seit Mitternacht)
void ds3231_set(uint32_t t);   // schreibt s.time in RTC
