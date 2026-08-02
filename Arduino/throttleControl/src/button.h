#pragma once

#define LONG_PRESS_TIME 100 //1.0 sec

void touch_button_setup(void);
void touch_button_handler(void);

// Struktur deklarieren (Definition in Header ist OK)
struct button
{
  bool released;
  bool pressed;
  uint16_t debounce;
  uint32_t time;
  uint32_t time1;
  uint32_t time2;
  int16_t msCount;
  int16_t msCount2;
  uint16_t msCountRelease; //for long press
};

// Variablen nur extern deklarieren, keine Initialisierung hier!
extern volatile button btnL;
extern volatile button btnR;