#pragma once

#include <Arduino.h>
#include "KumoServo.h"
#include "KumoKinematics.h"
#include "KumoGait.h"

class KumoMovement {
public:
    bool begin();
    void command(const String& command);
    void update(uint32_t nowMs);
    bool ready() const { return _ready; }

private:
    KumoServo _servos;
    KumoKinematics _ik;
    KumoGait _gait;
    bool _ready = false;
    bool _hasCommand = false;
    uint32_t _lastUpdate = 0;

    // Per-leg servo direction and trim. Change these after the first
    // supported-chassis calibration; do not assume mirrored servos match.
    float _direction[6][3] = {
        { 1,  1,  1}, { 1,  1,  1}, { 1,  1,  1},
        {-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1}
    };
    float _trim[6][3] = {};

    KumoMotion parseCommand(const String& command) const;
};
