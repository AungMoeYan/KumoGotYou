
#include "IR.h"

#define IR_PIN 26

void initIR() {
    pinMode(IR_PIN, INPUT);
}

bool readIR() {
    // Most digital IR obstacle modules output LOW
    // when an obstacle is detected.
    return digitalRead(IR_PIN) == LOW;
}
