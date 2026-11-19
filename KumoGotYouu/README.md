# KumoGotYou

KumoGotYou is a local Wi-Fi robot-control system built around two ESP32 boards and a Flask dashboard:

- **Kumo** is the main ESP32 controller for locomotion, servos, sensors, and dashboard communication.
- **Kumo Cam** is an AI-Thinker ESP32-CAM project that provides an MJPEG video stream.
- **KUMOGOTYOU_Server** is the Flask web dashboard, telemetry receiver, alert engine, and optional AI assistant.

> This is a hobby-robot project. Its stop button and software alerts are not substitutes for a hardware emergency stop, collision protection, or certified safety equipment.

## Project layout

```text
KumoGotYouu/
|-- Kumo/                 # Main ESP32 firmware (PlatformIO)
|-- Kumo Cam/             # ESP32-CAM firmware (PlatformIO)
|-- KUMOGOTYOU_Server/    # Flask dashboard and API
`-- README.md
```

## Requirements

- PlatformIO Core or the PlatformIO extension for VS Code
- A main ESP32 development board (`esp32dev` environment)
- An AI-Thinker ESP32-CAM (`esp32cam` environment) with PSRAM
- Python 3.9+ for the dashboard
- Two PCA9685 servo drivers at I2C addresses `0x40` and `0x41`
- An SSD1306 OLED at I2C address `0x3C`
- A shared Wi-Fi network for the computer, main ESP32, and ESP32-CAM

## Hardware connections used by the main controller

| Device | ESP32 pins / address |
| --- | --- |
| OLED SSD1306 | I2C SDA `21`, SCL `22`, address `0x3C` |
| PCA9685 servo drivers | I2C bus, addresses `0x40` and `0x41` |
| Ultrasonic sensor | Trigger `12`, Echo `13` |
| MQ-2 gas sensor (analog) | GPIO `34` |
| PIR sensor | GPIO `27` |
| IR obstacle sensor | GPIO `26` |

Use a suitable separate power supply for the servos, with a common ground to the ESP32. Do not power a multi-servo robot from the ESP32's USB/board regulator.

## Quick start

### 1. Start the dashboard

From `KUMOGOTYOU_Server`:

```powershell
py -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
```

Create a file named `.env` beside `app.py`:

```dotenv
# Optional: enables the Kumo AI features.
OPENROUTER_API_KEY=your_key_here

# ESP32-CAM stream endpoint; adjust to the camera's LAN address.
ESP32_CAM_URL=http://192.168.1.50:81

# Optional alert tuning.
ALERT_COOLDOWN=30
MQ2_ALERT_THRESHOLD=3000
OBSTACLE_ALERT_DISTANCE_CM=18
ANOMALY_CONFIRM_SAMPLES=3
```

Then run:

```powershell
py app.py
```

Open the local Flask URL printed in the terminal, normally `http://127.0.0.1:5000`.

### 2. Configure and upload the main ESP32

Open `Kumo` in PlatformIO. In `src/main.cpp`, configure:

- `KUMO_WIFI_SSID` and `KUMO_WIFI_PASSWORD`, preferably through private PlatformIO build flags rather than editing tracked source.
- `SERVER_URL` with the LAN address of the computer running Flask, for example `http://192.168.1.10:5000`.

The default values are placeholders and the sample server address will not work on another network. Build and upload the `esp32dev` environment, then open a serial monitor at `115200` baud.

### 3. Configure and upload the camera

Open `Kumo Cam` in PlatformIO. Configure its Wi-Fi credentials before building the `esp32cam` environment. After boot, note the IP address in the serial monitor (`115200` baud), then set `ESP32_CAM_URL` in the server `.env` to:

```text
http://<camera-ip>:81
```

The camera exposes still capture on port `80` and its MJPEG stream on port `81`.

## Dashboard API

The Flask server provides these main routes:

| Route | Purpose |
| --- | --- |
| `/` | Dashboard |
| `/robot_command` | Get or set robot movement commands |
| `/control` | Legacy servo-control endpoint |
| `/sensor_data` | Receive main-ESP32 sensor telemetry |
| `/status` | Server, robot, and camera status |
| `/camera` | Proxy the camera stream to the dashboard |
| `/ai` | Optional Kumo AI chat |
| `/ai_alert` | Read or submit alerts |
| `/debug` | Diagnostic state |

Supported movement commands are `forward`, `backward`, `left`, `right`, `turn_left`, `turn_right`, `stop`, `stand`, and `sit`.

An example sensor packet is:

```json
{
  "ultrasonic": {"distance": 12.4},
  "mq2": 820,
  "pir": false,
  "ir_obstacle": true
}
```

## Security and deployment notes

- Do not commit `.env`, Wi-Fi passwords, or API keys.
- The camera firmware currently keeps Wi-Fi credentials in source. Replace the exposed password, then move credentials to an ignored local configuration mechanism before sharing this project.
- The dashboard has no authentication. Use it only on a trusted local network; do not expose it directly to the public internet.
- Calibrate the MQ-2 threshold for the installed sensor. Its reading is raw ADC data, not a gas concentration or safety rating.
- Verify IR module polarity, servo directions/trims, and physical motion at low power before allowing autonomous or unattended movement.

## Troubleshooting

- **Dashboard shows robot offline:** Confirm `SERVER_URL` points to the computer's LAN IP, Flask is running, and all devices are on the same network.
- **Camera is unavailable:** Check the ESP32-CAM serial output, verify `ESP32_CAM_URL`, and open `http://<camera-ip>:81` from the dashboard computer.
- **AI says no key is configured:** Ensure `.env` is beside `app.py`, is named exactly `.env`, and restart Flask after editing it.
- **Servos move incorrectly:** Check the PCA9685 addresses/power supply and calibrate directions and trims in `Kumo/include/KumoMovement.h`.
