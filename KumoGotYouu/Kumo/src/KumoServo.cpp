#include "KumoServo.h"

namespace {
constexpr uint16_t SERVO_MIN_US = 900;
constexpr uint16_t SERVO_MAX_US = 2100;
constexpr float SERVO_MIN_DEG = 0.0f;
constexpr float SERVO_MAX_DEG = 180.0f;
constexpr float PWM_FREQUENCY_HZ = 50.0f;

// Logical joint order: coxa, femur, tibia.
// PCA #1 (0x40): channels 0..8.
// PCA #2 (0x41): channels 15..7, descending as requested.
constexpr uint8_t CHANNELS[6][3] = {
    {0, 1, 2},       // Front Left
    {3, 4, 5},       // Middle Left
    {6, 7, 8},       // Rear Left
    {15, 14, 13},    // Front Right
    {12, 11, 10},    // Middle Right
    {9, 8, 7}        // Rear Right
};
}

bool KumoServo::begin() {
    const bool leftOK = _leftBoard.begin();
    const bool rightOK = _rightBoard.begin();

    if (!leftOK || !rightOK) {
        Serial.printf("[SERVO] PCA9685 init failed: 0x40=%s, 0x41=%s\n",
            leftOK ? "OK" : "FAIL", rightOK ? "OK" : "FAIL");
        _ready = false;
        return false;
    }

    _leftBoard.setPWMFreq(PWM_FREQUENCY_HZ);
    _rightBoard.setPWMFreq(PWM_FREQUENCY_HZ);
    delay(10);

    _ready = true;
    Serial.println("[SERVO] PCA9685 boards ready at 0x40 and 0x41");
    Serial.println("[SERVO] Outputs remain untouched until a movement command arrives");
    return true;
}

uint16_t KumoServo::angleToPulse(float angleDeg) {
    angleDeg = constrain(angleDeg, SERVO_MIN_DEG, SERVO_MAX_DEG);
    const float pulse = SERVO_MIN_US +
        (angleDeg / 180.0f) * (SERVO_MAX_US - SERVO_MIN_US);
    return static_cast<uint16_t>(pulse + 0.5f);
}

void KumoServo::writeJoint(uint8_t leg, uint8_t joint, float angleDeg) {
    if (!_ready || leg >= 6 || joint >= 3) return;

    const uint16_t pulseUs = angleToPulse(angleDeg);
    if (leg < 3) {
        _leftBoard.writeMicroseconds(CHANNELS[leg][joint], pulseUs);
    } else {
        _rightBoard.writeMicroseconds(CHANNELS[leg][joint], pulseUs);
    }
}
