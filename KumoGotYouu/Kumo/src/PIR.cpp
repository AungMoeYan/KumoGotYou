
#include "PIR.h"

#define PIR_PIN 27

void initPIR() {
    pinMode(PIR_PIN, INPUT);
}

bool readPIR() {
    return digitalRead(PIR_PIN) == HIGH;
}
