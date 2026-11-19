KumoGotYou is developed to support remote observation and preliminary inspection in places that may be unsafe, difficult, or inconvenient for people to enter directly. Examples include smoke-filled rooms, narrow spaces, damaged structures, dark areas, and locations where obstacles or unknown environmental conditions may be present.

The robot allows an operator to observe an area from a safer distance through live video, sensor readings, and wireless remote control. Its six-legged hexapod design is intended to provide more stable movement than a simple wheeled robot when operating on uneven or irregular surfaces. With eighteen servo motors, the robot can perform controlled leg movements while maintaining multiple points of contact with the ground.

KumoGotYou collects environmental information using ultrasonic, gas, motion, and obstacle sensors. These readings help the operator understand nearby objects, possible smoke or combustible-gas conditions, movement around the robot, and other basic environmental changes. The ESP32-CAM provides a live visual feed, while the Flask dashboard combines the camera view, sensor values, robot status, movement controls, and alerts in one interface.

The project also demonstrates how embedded systems, robotics, Wi-Fi communication, web development, and AI-assisted monitoring can work together in a single practical prototype. The AI alert feature is intended to help summarize unusual sensor conditions for the operator; it does not replace human judgment or certified safety equipment.

Overall, the purpose of KumoGotYou is to create a low-cost prototype platform for learning and demonstrating remote robotics, environmental monitoring, surveillance, and search-and-rescue support concepts.

Our Project was featured in UCSY(University Of Computer Studies) Product Show and Job Fair 2026!!
<img width="1000" height="650" alt="photo_2026-11-19_13-24-20" src="https://github.com/user-attachments/assets/4bdc52ec-3b25-4986-87ac-b2bf5769c1ab" />

<img width="1000" height="650" alt="photo_2026-08-28_14-25-28" src="https://github.com/user-attachments/assets/7ee208ad-33a0-4809-a266-6b76e197232c" />

<img width="1000" height="650" alt="photo_2026-11-19_13-44-27" src="https://github.com/user-attachments/assets/918e99de-5800-4a96-bb98-743ea087b45a" />


## Main features

- Six-legged robot with 18 MG90S servo motors
- Basic movement control: forward, backward, left, right, stand, sit, and stop
- ESP32 main controller for movement, sensors, and Wi-Fi communication
- Two PCA9685 PWM servo drivers
- ESP32-CAM live video streaming
- Web-based monitoring and control interface
- Ultrasonic obstacle-distance monitoring
- MQ-2 smoke / combustible-gas monitoring
- PIR motion detection
- IR obstacle detection
- OLED screen for local status and selected sensor information
- AI-assisted alerts for abnormal sensor conditions

## System architecture

```text
Operator browser
      |
Flask dashboard / API
      |---------------- ESP32-CAM -> live video stream
      |
Main ESP32 -> PCA9685 drivers -> 18 servos / six legs
      |
      +-> ultrasonic, MQ-2, PIR, IR, and OLED modules

## Project folders

- Kumo/ — Main ESP32 firmware for movement, sensors, OLED output, and Flask communication.
- Kumo Cam/ — ESP32-CAM firmware for capture and MJPEG streaming.
- KUMOGOTYOU_Server/ — Flask server, dashboard UI, telemetry, alerts, and optional AI integration.


## Running the system

1. Start the Flask server in KUMOGOTYOU_Server.
2. Connect the main ESP32, ESP32-CAM, and computer to the same Wi-Fi network.
3. Set the main ESP32’s Wi-Fi credentials and Flask server LAN address.
4. Upload the main firmware from Kumo.
5. Upload the camera firmware from Kumo Cam.
6. Set the server camera URL to the ESP32-CAM’s local address.
7. Open the Flask dashboard in a browser.

## Scope and limitations

KumoGotYou supports prototype-level remote monitoring, environmental sensing, video observation, and operator-controlled movement. Sensor values and AI alerts provide supporting information only; they are not certified safety, gas-detection, or emergency-response systems.

## Future improvements

- Smoother and more stable gait control
- Better power monitoring and operating time
- Additional obstacle sensors
- More reliable video streaming
- Autonomous or semi-autonomous navigation
- Mapping and localization
- Expanded AI-assisted environmental analysis


- **Kumo** is the main ESP32 controller for locomotion, servos, sensors, and dashboard communication.
- **Kumo Cam** is a ESP32-CAM that provides a video stream.
- **KUMOGOTYOU_Server** is the Flask web dashboard, telemetry receiver, alert engine, and optional AI assistant.

## Requirements

- PlatformIO Core or the PlatformIO extension for VS Code
- A main ESP32 development board (esp32dev` environment)
- A ESP32-CAM (esp32cam environment)
- Python 3.9+ for the dashboard
- Two PCA9685 servo drivers at I2C addresses 0x40 and 0x41
- An SSD1306 OLED at I2C address 0x3C
- A shared Wi-Fi network for the computer, main ESP32, and ESP32-CAM

## Hardware connections used by the main controller

Hardware requirements

- Esp 32 development board x1
- Esp 32 cam with base x1
- A button
- 2 step-down transformers(LMS2596 serie)
- 2 step-up     transformer(XL6009E1 serie)
- 2 pca9685 servo drivers 
- MQ2 gas sensor x1
- Ultrasonic sensor x1
- IR obstacle detector x1
- Pir module x1
- 0.96 OLED display module
- 2 x lithium polymer battery 
- 3 x li-ion battery
- A parallel battery holder 
- A bunch of jumper wires
- A 3D printed Hexapod model

Tools requirements 
- Glue gun
- Multimeter
- soldering material
- soldering gun

## Quick start

### 1. Start the dashboard

From KUMOGOTYOU_Server:

py -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt

Create a file named .env beside app.py:

# Optional: enables the Kumo AI features.
OPENROUTER_API_KEY=your_key_here

# ESP32-CAM stream endpoint; adjust to the camera's LAN address.
ESP32_CAM_URL=<address>

# Optional alert tuning.
ALERT_COOLDOWN=30
MQ2_ALERT_THRESHOLD=3000
OBSTACLE_ALERT_DISTANCE_CM=18
ANOMALY_CONFIRM_SAMPLES=3

Then run:

py app.py

Open the local Flask URL printed in the terminal, normally http://127.0.0.1:5000.

### 2. Configure and upload the main ESP32

Open Kumo in PlatformIO. In src/main.cpp, configure:

- KUMO_WIFI_SSID and KUMO_WIFI_PASSWORD, preferably through private PlatformIO build flags rather than editing tracked source.
- SERVER_URL with the LAN address of the computer running Flask, for example http://192.168.1.10:5000.

The default values are placeholders and the sample server address will not work on another network. Build and upload the esp32dev environment, then open a serial monitor at 115200 baud.

### 3. Configure and upload the camera

Open Kumo Cam in PlatformIO. Configure its Wi-Fi credentials before building the esp32cam environment. After boot, note the IP address in the serial monitor (115200 baud), then set ESP32_CAM_URL in the server .env to:

http://<camera-ip>:81

The camera exposes still capture on port 80 and its MJPEG stream on port 81.

## Dashboard API

The Flask server provides these main routes:

| Route | Purpose |
| --- | --- |
| / | Dashboard |
| /robot_command | Get or set robot movement commands |
| /control | Legacy servo-control endpoint |
| /sensor_data | Receive main-ESP32 sensor telemetry |
| /status | Server, robot, and camera status |
| /camera | Proxy the camera stream to the dashboard |
| /ai | Optional Kumo AI chat |
| /ai_alert | Read or submit alerts |
| /debug | Diagnostic state |

Supported movement commands are forward, backward, left, right, turn_left, turn_right, stop, stand, and sit.

An example sensor packet is:

{
  "ultrasonic": {"distance": 12.4},
  "mq2": 820,
  "pir": false,
  "ir_obstacle": true
}
