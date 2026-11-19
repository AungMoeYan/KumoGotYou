#include "KumoKinematics.h"
#include <math.h>

namespace {
constexpr float RAD_TO_DEG_F = 57.2957795131f;
constexpr float DEG_TO_RAD_F = 0.0174532925199f;

// Leg order: Front Left, Middle Left, Rear Left,
//            Front Right, Middle Right, Rear Right.
// Neutral foot vectors are relative to each coxa axis in the body XY plane.
// They are initial estimates and intentionally easy to tune.
constexpr float NEUTRAL_X[6] = {55.0f, 0.0f, -55.0f, 55.0f, 0.0f, -55.0f};
constexpr float NEUTRAL_Y[6] = {60.0f, 65.0f, 60.0f, -60.0f, -65.0f, -60.0f};
}

void KumoKinematics::getNeutralFoot(uint8_t leg, float& x, float& y, float& z) const {
    if (leg >= 6) { x = y = z = 0.0f; return; }
    x = NEUTRAL_X[leg];
    y = NEUTRAL_Y[leg];
    z = footHeight;
}

bool KumoKinematics::solveRaw(uint8_t leg, float footX, float footY, float footZ,
                              float& coxa, float& femur, float& tibia) const {
    if (leg >= 6 || coxaLength <= 0 || femurLength <= 0 || tibiaLength <= 0) return false;

    const float radial = sqrtf(footX * footX + footY * footY);
    if (radial <= coxaLength + 1.0f) return false;

    coxa = atan2f(footY, footX) * RAD_TO_DEG_F;
    const float planar = radial - coxaLength;
    const float distance = sqrtf(planar * planar + footZ * footZ);
    const float minReach = fabsf(femurLength - tibiaLength) + 1.0f;
    const float maxReach = femurLength + tibiaLength - 1.0f;
    if (distance < minReach || distance > maxReach) return false;

    const float beta = atan2f(footZ, planar);
    float cosAlpha = (femurLength * femurLength + distance * distance -
                      tibiaLength * tibiaLength) / (2.0f * femurLength * distance);
    float cosKnee = (femurLength * femurLength + tibiaLength * tibiaLength -
                     distance * distance) / (2.0f * femurLength * tibiaLength);
    cosAlpha = constrain(cosAlpha, -1.0f, 1.0f);
    cosKnee = constrain(cosKnee, -1.0f, 1.0f);

    const float alpha = acosf(cosAlpha);
    const float kneeInternal = acosf(cosKnee);
    femur = (beta - alpha) * RAD_TO_DEG_F;
    tibia = (PI - kneeInternal) * RAD_TO_DEG_F;
    return true;
}

bool KumoKinematics::solve(uint8_t leg, float footX, float footY, float footZ,
                           KumoJointAngles& result) const {
    float neutralX, neutralY, neutralZ;
    float baseCoxa, baseFemur, baseTibia;
    getNeutralFoot(leg, neutralX, neutralY, neutralZ);
    if (!solveRaw(leg, neutralX, neutralY, neutralZ,
                  baseCoxa, baseFemur, baseTibia)) {
        result.reachable = false;
        return false;
    }

    float coxa, femur, tibia;
    if (!solveRaw(leg, footX, footY, footZ, coxa, femur, tibia)) {
        result.reachable = false;
        return false;
    }

    // Output is a delta from the computed neutral pose. The servo horn's
    // physical 90-degree centre is the configurable calibration reference.
    float coxaDelta = coxa - baseCoxa;
    while (coxaDelta > 180.0f) coxaDelta -= 360.0f;
    while (coxaDelta < -180.0f) coxaDelta += 360.0f;

    result.coxa = 90.0f + coxaDelta;
    result.femur = 90.0f + (femur - baseFemur);
    result.tibia = 90.0f + (tibia - baseTibia);
    result.reachable = true;
    return true;
}
