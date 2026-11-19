#pragma once

#include <Arduino.h>
#include <Adafruit_PWMServoDriver.h>

class KumoServo {
public:
    bool begin();
    void writeJoint(uint8_t leg, uint8_t joint, float angleDeg);
    bool ready() const { return _ready; }

private:
    Adafruit_PWMServoDriver _leftBoard{0x40};
    Adafruit_PWMServoDriver _rightBoard{0x41};
    bool _ready = false;

    static uint16_t angleToPulse(float angleDeg);
};
