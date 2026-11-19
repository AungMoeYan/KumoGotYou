

## Channel map
| Leg          | Board | Coxa | Femur | Tibia |
| ------------ | ----: | ---: | ----: | ----: |
| Front Left   |  0x40 |    0 |     1 |     2 |
| Middle Left  |  0x40 |    3 |     4 |     5 |
| Rear Left    |  0x40 |    6 |     7 |     8 |
| Front Right  |  0x41 |   15 |    14 |    13 |
| Middle Right |  0x41 |   12 |    11 |    10 |
| Rear Right   |  0x41 |    9 |     8 |     7 |

## Geometry currently configured (mm)
- Coxa 30, femur 65, tibia 110.
- Body length 165, body width 100.
- Front/middle/rear body-centre offsets 65/55/65.
- Left/right half-width 50.
- Neutral foot vectors are initial software estimates in `KumoKinematics.cpp`; adjust after verifying the physical assembly.

## Commands recognized
`forward`, `backward`, `left`, `right`, `turn_left`, `turn_right`, `stop`.
Aliases include `up`, `down`, `reverse`, `move_forward`, `move_backward`, `strafe_left`, `strafe_right`, `rotate_left`, and `rotate_right`. Unknown commands are treated as `stop`.

## Important calibration and first test
1. The existing archive did not contain the Flask dashboard/backend or its JavaScript, so the exact strings sent by the dashboard could not be checked. Confirm its command strings match the list above.
2. Servo direction, trim, neutral foot height, and servo pulse endpoints are provisional. They are configurable in `KumoMovement.h`, `KumoKinematics.h`, and `KumoServo.cpp`.
3. Servo outputs are not written at boot. They begin only after the first received movement command.
4. The current output angle is deliberately clamped to 70–110 degrees as a conservative first-pass limit. This is not a completed calibration.
5. For first tests, disconnect the leg links or support the chassis so the feet are clear, then test one joint/channel at a time. Do not run a walking gait until channel mapping, direction, and neutral positions have been checked.
6. Both PCA9685 boards must share the ESP32 I2C bus (`Wire`, SDA 21, SCL 22) and have separate addresses 0x40 and 0x41. Use a suitable external servo power supply with common ground; do not power 18 servos from the ESP32.

## Wi-Fi configuration
The original source contained a hard-coded Wi-Fi password. It has been replaced with placeholders. Set `KUMO_WIFI_SSID` and `KUMO_WIFI_PASSWORD` in PlatformIO build flags, or edit the two macros near the top of `src/main.cpp` before flashing. Update `SERVER_URL` if your Flask host address has changed.


