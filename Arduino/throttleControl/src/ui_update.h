#ifndef UI_UPDATE_H
#define UI_UPDATE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ui_update()
 * -----------
 * Aktualisiert die LVGL-UI abhängig von der aktuell geladenen Page (menuState/10)
 * und nur dann, wenn sich ein darzustellender Wert (p.* oder s.*) oder die Page
 * selbst geändert hat. Beim Page-Wechsel werden alle Widgets der neuen Page
 * einmal zwangsaktualisiert.
 *
 * Voraussetzungen:
 *   - globale Variablen in main.cpp:  EBikeSettings p;  EBikeStatus s;
 *   - globale Variable in menu.cpp:   extern uint8_t menuState;
 *   - LVGL-Objekte via screens.h (objects_t objects)
 *
 * Aufruf periodisch im Hauptloop, z.B. alle 20-100 ms.
 */
void ui_update(void);

#ifdef __cplusplus
}
#endif

#endif // UI_UPDATE_H