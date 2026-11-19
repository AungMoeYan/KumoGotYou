
#include "MQ2.h"

#define MQ2_PIN 34

void initMQ2() {
    pinMode(MQ2_PIN, INPUT);
}

int readMQ2() {
    return analogRead(MQ2_PIN);
}
