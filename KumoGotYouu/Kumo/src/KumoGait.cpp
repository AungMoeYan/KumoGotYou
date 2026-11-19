#include "KumoGait.h"
#include <math.h>

namespace {
constexpr float TWO_PI_F = 6.28318530718f;
constexpr float HALF_PI_F = 1.57079632679f;
// Tripod A: front-left, middle-right, rear-left.
constexpr bool TRIPOD_A[6] = {true, false, true, false, true, false};
constexpr float LEG_X[6] = {65.0f, 0.0f, -65.0f, 65.0f, 0.0f, -65.0f};
constexpr float LEG_Y[6] = {50.0f, 50.0f, 50.0f, -50.0f, -50.0f, -50.0f};
}

void KumoGait::setMotion(KumoMotion motion, uint32_t nowMs) {
    if (_motion != motion) {
        _motion = motion;
        _startedAt = nowMs;
    }
}

KumoFootOffset KumoGait::getOffset(uint8_t leg, uint32_t nowMs) const {
    KumoFootOffset out{0.0f, 0.0f, 0.0f};
    if (leg >= 6 || _motion == KumoMotion::Stop || cycleMs == 0) return out;

    float phase = fmodf(static_cast<float>(nowMs - _startedAt) /
                        static_cast<float>(cycleMs), 1.0f);
    if (!TRIPOD_A[leg]) phase = fmodf(phase + 0.5f, 1.0f);

    const bool swing = phase < 0.5f;
    const float p = swing ? phase * 2.0f : (phase - 0.5f) * 2.0f;
    const float foreAft = swing
        ? (strideMm * 0.5f - strideMm * p)
        : (-strideMm * 0.5f + strideMm * p);
    const float lift = swing ? sinf(p * PI) * liftMm : 0.0f;

    float directionX = 0.0f, directionY = 0.0f;
    float rotation = 0.0f;
    switch (_motion) {
        case KumoMotion::Forward: directionX = 1.0f; break;
        case KumoMotion::Backward: directionX = -1.0f; break;
        case KumoMotion::Left: directionY = 1.0f; break;
        case KumoMotion::Right: directionY = -1.0f; break;
        case KumoMotion::TurnLeft: rotation = 1.0f; break;
        case KumoMotion::TurnRight: rotation = -1.0f; break;
        default: break;
    }

    if (rotation != 0.0f) {
        out.x = foreAft * (-LEG_Y[leg] / 50.0f) * rotation;
        out.y = foreAft * ( LEG_X[leg] / 65.0f) * rotation;
    } else {
        out.x = foreAft * directionX;
        out.y = foreAft * directionY;
    }
    out.z = -lift; // Positive foot Z is downward in the IK coordinate system.
    return out;
}
