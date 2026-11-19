#pragma once

#include <Arduino.h>

struct KumoJointAngles {
    float coxa;
    float femur;
    float tibia;
    bool reachable;
};

class KumoKinematics {
public:
    // Lengths are millimetres. Foot X is forward, Y is left, Z is downward.
    bool solve(uint8_t leg, float footX, float footY, float footZ,
               KumoJointAngles& result) const;
    void getNeutralFoot(uint8_t leg, float& x, float& y, float& z) const;

    float coxaLength = 30.0f;
    float femurLength = 65.0f;
    float tibiaLength = 110.0f;
    float footHeight = 100.0f;

private:
    bool solveRaw(uint8_t leg, float footX, float footY, float footZ,
                  float& coxa, float& femur, float& tibia) const;
};
