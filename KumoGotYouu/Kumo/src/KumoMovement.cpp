#include "KumoMovement.h"
#include <math.h>

bool KumoMovement::begin() {
    _ready = _servos.begin();
    if (!_ready) {
        Serial.println("[MOVEMENT] Disabled: PCA9685 initialization failed");
        return false;
    }
    Serial.println("[MOVEMENT] IK + tripod gait ready; waiting for command");
    return true;
}

KumoMotion KumoMovement::parseCommand(const String& raw) const {
    String c = raw;
    c.trim();
    c.toLowerCase();

    if (c == "forward" || c == "move_forward" || c == "up") return KumoMotion::Forward;
    if (c == "backward" || c == "reverse" || c == "move_backward" || c == "down") return KumoMotion::Backward;
    if (c == "left" || c == "strafe_left" || c == "move_left") return KumoMotion::Left;
    if (c == "right" || c == "strafe_right" || c == "move_right") return KumoMotion::Right;
    if (c == "turn_left" || c == "rotate_left") return KumoMotion::TurnLeft;
    if (c == "turn_right" || c == "rotate_right") return KumoMotion::TurnRight;
    return KumoMotion::Stop;
}

void KumoMovement::command(const String& commandText) {
    const KumoMotion next = parseCommand(commandText);
    _gait.setMotion(next, millis());
    _hasCommand = true;
    Serial.printf("[MOVEMENT] Command: %s\n", commandText.c_str());
}

void KumoMovement::update(uint32_t nowMs) {
    if (!_ready || !_hasCommand || nowMs - _lastUpdate < 20) return;
    _lastUpdate = nowMs;

    for (uint8_t leg = 0; leg < 6; ++leg) {
        float neutralX, neutralY, neutralZ;
        _ik.getNeutralFoot(leg, neutralX, neutralY, neutralZ);
        const KumoFootOffset offset = _gait.getOffset(leg, nowMs);

        KumoJointAngles angles;
        if (!_ik.solve(leg,
                       neutralX + offset.x,
                       neutralY + offset.y,
                       neutralZ + offset.z,
                       angles)) {
            // Reject unreachable targets instead of commanding a guessed pose.
            continue;
        }

        const float values[3] = {angles.coxa, angles.femur, angles.tibia};
        for (uint8_t joint = 0; joint < 3; ++joint) {
            const float calibrated = 90.0f +
                (values[joint] - 90.0f) * _direction[leg][joint] +
                _trim[leg][joint];
            _servos.writeJoint(leg, joint, constrain(calibrated, 70.0f, 110.0f));
        }
    }
}
