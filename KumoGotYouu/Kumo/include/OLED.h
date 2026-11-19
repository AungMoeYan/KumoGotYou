
#ifndef OLED_H
#define OLED_H

#include <Arduino.h>

bool initOLED();

void updateOLED(
    const char* status,
    int distanceCm,
    int mq2Value,
    bool pirDetected,
    bool irDetected
);

void clearOLED();

#endif
