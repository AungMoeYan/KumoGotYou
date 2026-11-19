# KumoGotYou Dashboard

This version keeps the original KumoGotYou HTML/CSS/JS structure and visual style. The local-area map and all related controls have been removed. No GPS or geolocation features are included.

## Run

1. Install Python dependencies: `pip install -r requirements.txt`
2. Copy `.env.example` to a new file named exactly `.env` in the same folder as `app.py` (do not leave it named `.env.example`).
3. Open `.env` and replace the placeholder with your newly generated key, keeping the variable name exactly `OPENROUTER_API_KEY`.
4. Set `ESP32_CAM_URL` to the address of your ESP32-CAM if it differs from the default.
5. Run `python app.py` and open the local Flask address shown in the terminal.

## API compatibility

The existing routes and command payloads are retained: `/robot_command`, `/control`, `/sensor_data`, `/status`, `/camera`, `/ai`, and `/ai_alert`. Robot-side behavior still depends on matching ESP32 firmware.

## Security

The API key present in the uploaded source has been removed from this package because it should be treated as exposed. Revoke/rotate it in your provider account and use the replacement only in your local `.env`. Do not commit `.env`.


## Refined dashboard UI

- Removed both on-screen servo joysticks and their browser-side `/control` requests.
- Kept directional movement controls, keyboard controls, stand/sit presets, and the stop command.
- Kept live camera, sensor telemetry, connection indicators, and Kumo AI.
- Added a telemetry refresh timestamp and refined the responsive three-column layout.
- The dashboard's STOP ROBOT button sends the existing `stop` command; it is not a certified hardware emergency-stop circuit.

The existing Flask `/control` route is left intact for compatibility with any other client, but this dashboard no longer uses it.

## If Kumo AI says the key is not configured

- Confirm the file is named exactly `.env`, not `.env.example` or `.env.txt`.
- Confirm `.env` is in the same `KUMOGOTYOU` folder as `app.py`.
- Its first line should look like `OPENROUTER_API_KEY=your_actual_key` (do not paste the key into chat).
- Install dependencies with `pip install -r requirements.txt`.
- Stop Flask completely and start it again with `python app.py`; the key is read when the server starts.
- On startup, the terminal reports whether a key was detected without printing the key itself.

## Automatic sensor alerts and Kumo voice

The Flask backend now evaluates each incoming `/sensor_data` packet locally. It can raise an alert after three consecutive readings (configurable) for:

- Ultrasonic distance below `OBSTACLE_ALERT_DISTANCE_CM` (default 18 cm).
- PIR motion state reported as `true`.
- IR obstacle state reported as `true`.
- MQ-2 raw ADC readings at/above `MQ2_ALERT_THRESHOLD` (default 3000).

When a rule triggers, the dashboard immediately shows a pending alert. A background task asks OpenRouter for a short explanation and updates the alert when the response arrives. The ESP32 sensor POST is not held open while the AI request runs. Repeated alerts are limited by `ALERT_COOLDOWN` seconds.

### Important sensor calibration notes

- The MQ-2 threshold is a **raw ADC reading**, not ppm. The default `3000` value is only a starting point; observe your board's normal readings and adjust `MQ2_ALERT_THRESHOLD` in `.env`. Do not use this dashboard as a certified gas detector or life-safety alarm.
- The PIR rule assumes your firmware sends `pir: true` when motion is detected; ordinary movement can also trigger it.
- The IR rule assumes your firmware sends `ir_obstacle: true` when an obstacle is detected. If your module uses the opposite output polarity, correct that mapping in the ESP32 firmware.
- Ultrasonic readings of zero are ignored by the close-obstacle rule. Check your sensor's actual readings and wiring before relying on the threshold.
- The rules require the ESP32 to keep POSTing sensor readings to `/sensor_data`; the Flask server cannot infer sensor events if the firmware is not sending them.

### Voice

Click **VOICE OFF** in the Kumo AI header to enable **VOICE ON**. The browser's built-in speech synthesis reads new automatic alerts and Kumo's chat replies aloud. Voice is generated locally by the browser; it does not use another paid AI API. Keep the dashboard tab open and allow audio. Speech availability and voice quality depend on the browser and installed system voices. Voice alerts are off by default until enabled by a user gesture.

### Sensor payload example

```json
{
  "ultrasonic": {"distance": 12.4},
  "mq2": 820,
  "pir": false,
  "ir_obstacle": true
}
```

The JSON keys must match the fields your current ESP32 firmware actually sends. This dashboard update does not replace or automatically modify the ESP32 firmware.
