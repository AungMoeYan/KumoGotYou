#ifndef ULTRASONIC_FILTER_H
#define ULTRASONIC_FILTER_H

#include <Arduino.h>

extern const int PIN_TRIG;
extern const int PIN_ECHO;
extern const size_t FILTER_SIZE;

extern volatile uint32_t echoStart;
extern volatile uint32_t echoEnd;
extern volatile bool pulseReady;

void IRAM_ATTR echoISR();
float getFilteredDistance();

#endif
