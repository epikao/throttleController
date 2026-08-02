#include <Arduino.h>
#include "globals.h"  
#include "button.h"


/* Touch button Initialisierung (Reihenfolge: released, pressed, debounce, ...) */
volatile button btnL = {false, false, 70, 0, 0, 0, -1, -1, LONG_PRESS_TIME};
volatile button btnR = {false, false, 70, 0, 0, 0, -1, -1, LONG_PRESS_TIME};

void touch_button_setup(){
  pinMode(TOUCH_L_PIN, INPUT);
  pinMode(TOUCH_R_PIN, INPUT);
}

/**
 * Interne Hilfsfunktion zur Verarbeitung der Button-Logik um Duplikate zu vermeiden.
 */
static void update_button_internal(volatile button &btn, bool current_read) {
  if (current_read) {
    if (!btn.pressed) btn.time1 = millis();
    btn.pressed = true;
    btn.time = millis();
    
    if ((millis() - btn.time2) > 10) {
      btn.msCount++;
      btn.msCount2++;
      btn.time2 = millis();
    }
    
    if (btn.msCount2 >= btn.msCountRelease) {
      btn.released = true;
      btn.msCount2 = 0;
    }
    
    // Dynamische Release-Rate für Long-Press
    if (btn.msCount > 750) btn.msCountRelease = 1;
    else if (btn.msCount > 500) btn.msCountRelease = 10;
    else if (btn.msCount > 200) btn.msCountRelease = 30;

  } else if (((millis() - btn.time) > btn.debounce) && btn.pressed) {
    // Button wurde losgelassen (Release-Event nach Debounce)
    btn.released = true;
    btn.pressed = false;
    btn.msCountRelease = LONG_PRESS_TIME;
    btn.msCount = -1;
    btn.msCount2 = -1;
  }
}

void touch_button_handler(){
  update_button_internal(btnL, digitalRead(TOUCH_L_PIN));
  update_button_internal(btnR, digitalRead(TOUCH_R_PIN));
}

void button_test(){
        if(btnL.released == 1){
            Serial.print("btnL:"); Serial.println(btnL.released);
            btnL.released = 0;
        }
        if(btnR.released == 1){
            Serial.print("btnR:"); Serial.println(btnR.released);
            btnR.released = 0;
        }
        if( (btnR.msCount > LONG_PRESS_TIME) && (btnL.msCount > LONG_PRESS_TIME) ){
            Serial.print("btnRandL_long");
            btnR.msCount = -1;
            btnL.msCount = -1;
        }
}