#pragma once

#include <Arduino.h>

enum class KumoMotion : uint8_t {
    Stop, Forward, Backward, Left, Right, TurnLeft, TurnRight
};

struct KumoFootOffset {
    float x;
    float y;
    float z;
};

class KumoGait {
public:
    void setMotion(KumoMotion motion, uint32_t nowMs);
    KumoFootOffset getOffset(uint8_t leg, uint32_t nowMs) const;
    KumoMotion motion() const { return _motion; }

    float strideMm = 24.0f;
    float liftMm = 20.0f;
    uint32_t cycleMs = 1200;

private:
    KumoMotion _motion = KumoMotion::Stop;
    uint32_t _startedAt = 0;
};
