#pragma once

void menu_handler(void);
void stopwatch_update(void); // Stopwatch-Ticks auf Core 0 ueber millis()-Delta

extern uint8_t menuState;
extern volatile uint8_t stopwatchState; // 0=idle, 1=running, 2=stopped