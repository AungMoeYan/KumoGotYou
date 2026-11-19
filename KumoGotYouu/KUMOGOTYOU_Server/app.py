from flask import Flask, render_template, request, jsonify, Response
from dotenv import load_dotenv
from pathlib import Path
import os
import requests
import time
import threading

load_dotenv(dotenv_path=Path(__file__).resolve().parent / ".env")

app = Flask(__name__)

# ============================================================
# CONFIG
# ============================================================

OPENROUTER_API_KEY = os.getenv("OPENROUTER_API_KEY", "").strip()

AI_MODEL = "openai/gpt-4o-mini"

if OPENROUTER_API_KEY:
    print("[KumoGotYou] OpenRouter API key detected.")
else:
    print("[KumoGotYou] OpenRouter API key NOT detected. Check the .env file beside app.py.")

ESP32_CAM_URL = os.getenv("ESP32_CAM_URL", "http://10.15.5.160:81").rstrip("/")

ESP32_TIMEOUT = 5

ALERT_COOLDOWN = int(os.getenv("ALERT_COOLDOWN", "30"))
# Raw ADC thresholds are configurable and MUST be calibrated for your modules.
# These defaults are conservative starting points, not ppm or safety limits.
MQ2_ALERT_THRESHOLD = float(os.getenv("MQ2_ALERT_THRESHOLD", "3000"))
OBSTACLE_ALERT_DISTANCE_CM = float(os.getenv("OBSTACLE_ALERT_DISTANCE_CM", "18"))
ANOMALY_CONFIRM_SAMPLES = max(1, int(os.getenv("ANOMALY_CONFIRM_SAMPLES", "3")))

# ============================================================
# COMMAND STATE
# ============================================================

latest_command = "stop"

command_id = 0

last_command_time = time.time()

ALLOWED_COMMANDS = [
    "forward",
    "backward",
    "left",
    "right",
    "turn_left",
    "turn_right",
    "stop",
    "stand",
    "sit"
]

# ============================================================
# JOYSTICK
# ============================================================

latest_control = {

    "type": "servo_controller",

    "upper": {
        "x": 0.0,
        "y": 0.0
    },

    "lower": {
        "x": 0.0,
        "y": 0.0
    }
}

last_control_time = time.time()

# ============================================================
# SENSOR
# ============================================================

latest_sensor_data = {

    "ultrasonic": {
        "distance": 0.0
    },

    "mq2": 0,

    "pir": False,

    "movement": 0,

    "ir_obstacle": False,

    "ir_raw": 1
}

last_sensor_time = 0

camera_online = False
last_camera_ok = 0
CAMERA_STATUS_TIMEOUT = 5

# ============================================================
# AI ALERT
# ============================================================

last_alert_type = None
last_alert_time = 0

current_ai_alert = {
    "alert": False,
    "pending": False,
    "type": "",
    "response": "",
    "time": 0,
    "id": 0
}

_alert_lock = threading.Lock()
_alert_sequence = 0
_anomaly_counts = {"obstacle": 0, "mq2": 0, "pir": 0, "ir": 0}


# ============================================================
# MAIN
# ============================================================

@app.route("/")
def index():

    return render_template(
        "index.html"
    )


# ============================================================
# COMMAND
# ============================================================

@app.route(
    "/robot_command",
    methods=["GET", "POST"]
)
def robot_command():

    global latest_command
    global command_id
    global last_command_time

    # --------------------------------------------------------
    # ESP32 GET
    # --------------------------------------------------------

    if request.method == "GET":

        return jsonify({

            "id": command_id,

            "command":
                latest_command,

            "time":
                last_command_time
        })

    # --------------------------------------------------------
    # WEB POST
    # --------------------------------------------------------

    data = request.get_json(
        silent=True
    )

    if not data:

        return jsonify({
            "status": "error",
            "message": "Invalid JSON"
        }), 400

    command = str(
        data.get(
            "command",
            ""
        )
    ).strip().lower()

    if command not in ALLOWED_COMMANDS:

        return jsonify({

            "status": "error",

            "message":
                "Unknown command",

            "allowed":
                ALLOWED_COMMANDS

        }), 400

    latest_command = command

    command_id += 1

    last_command_time = time.time()

    print(
        f"[COMMAND #{command_id}] "
        f"{latest_command}"
    )

    return jsonify({

        "status":
            "ok",

        "id":
            command_id,

        "command":
            latest_command
    })


# ============================================================
# JOYSTICK
# ============================================================

@app.route(
    "/control",
    methods=["GET", "POST"]
)
def control():

    global last_control_time

    if request.method == "POST":

        data = request.get_json(
            silent=True
        )

        if not data:

            return jsonify({
                "status": "error",
                "message": "Invalid JSON"
            }), 400

        if data.get(
            "type"
        ) != "servo_controller":

            return jsonify({
                "status": "error",
                "message":
                    "Invalid controller type"
            }), 400

        group = data.get(
            "group"
        )

        if group not in [
            "upper",
            "lower"
        ]:

            return jsonify({
                "status": "error",
                "message":
                    "Invalid group"
            }), 400

        try:

            x = float(
                data.get(
                    "x",
                    0
                )
            )

            y = float(
                data.get(
                    "y",
                    0
                )
            )

        except (
            ValueError,
            TypeError
        ):

            return jsonify({
                "status": "error",
                "message":
                    "Invalid joystick"
            }), 400

        x = max(
            -1.0,
            min(1.0, x)
        )

        y = max(
            -1.0,
            min(1.0, y)
        )

        latest_control[
            group
        ]["x"] = x

        latest_control[
            group
        ]["y"] = y

        last_control_time = time.time()

        print(
            "[JOYSTICK]",
            group,
            x,
            y
        )

        return jsonify({

            "status":
                "ok",

            "control":
                latest_control
        })

    return jsonify(
        latest_control
    )


# ============================================================
# LOCAL ANOMALY DETECTION + ASYNC AI EXPLANATION
# ============================================================

def _generate_ai_alert(alert_type, reason):
    """Run the network AI request off the ESP32 sensor-ingest request."""
    global current_ai_alert
    message = f"""The robot detected an unusual sensor event.
Event: {alert_type}
Reason: {reason}
Analyze the supplied live readings. Explain briefly what changed, which sensor reported it, and a sensible cautious response. Do not claim this is a confirmed hazard."""
    answer, error = ask_ai(message, True)
    if error or not answer:
        answer = f"I detected {reason}. Please check the sensor and surrounding area. My AI analysis service is currently unavailable, so this is a sensor-based warning only."
    with _alert_lock:
        current_ai_alert.update({"pending": False, "response": answer, "error": bool(error)})
    print(f"[KUMO AI ALERT] {alert_type}: {answer}")


def _trigger_local_anomaly(alert_type, reason):
    global last_alert_type, last_alert_time, current_ai_alert, _alert_sequence
    now = time.time()
    with _alert_lock:
        if alert_type == last_alert_type and now - last_alert_time < ALERT_COOLDOWN:
            return False
        last_alert_type = alert_type
        last_alert_time = now
        _alert_sequence += 1
        current_ai_alert = {
            "alert": True, "pending": True, "type": alert_type,
            "response": f"I detected {reason}. Analysing the sensor readings now.",
            "time": now, "id": _alert_sequence, "error": False
        }
    threading.Thread(target=_generate_ai_alert, args=(alert_type, reason), daemon=True).start()
    print(f"[LOCAL ANOMALY] {alert_type}: {reason}")
    return True


def evaluate_sensor_anomalies():
    """Rule-based screening on each sensor packet; thresholds are configurable."""
    distance = latest_sensor_data["ultrasonic"].get("distance", 0)
    mq2 = latest_sensor_data.get("mq2", 0)
    pir = latest_sensor_data.get("pir", False)
    ir = latest_sensor_data.get("ir_obstacle", False)

    checks = {
        "obstacle": (isinstance(distance, (int, float)) and 0 < distance < OBSTACLE_ALERT_DISTANCE_CM,
                     f"an object is very close: ultrasonic distance is {distance:.1f} cm"),
        "mq2": (isinstance(mq2, (int, float)) and mq2 >= MQ2_ALERT_THRESHOLD,
                f"MQ-2 raw reading is high ({mq2:.0f}; configured trigger {MQ2_ALERT_THRESHOLD:.0f})"),
        "pir": (bool(pir), "the PIR motion sensor has detected movement"),
        "ir": (bool(ir), "the IR obstacle sensor reports an obstacle")
    }
    for key, (active, reason) in checks.items():
        _anomaly_counts[key] = _anomaly_counts.get(key, 0) + 1 if active else 0
        if _anomaly_counts[key] == ANOMALY_CONFIRM_SAMPLES:
            labels = {"obstacle": "Close obstacle", "mq2": "MQ-2 high reading", "pir": "Motion detected", "ir": "IR obstacle"}
            _trigger_local_anomaly(labels[key], reason)


# ============================================================
# SENSOR
# ============================================================

@app.route(
    "/sensor_data",
    methods=["GET", "POST"]
)
def sensor_data():

    global last_sensor_time

    if request.method == "POST":

        data = request.get_json(
            silent=True
        )

        if not data:

            return jsonify({
                "status": "error",
                "message":
                    "Invalid JSON"
            }), 400

        print(
            "[SENSOR]",
            data
        )

        # ----------------------------------------------------
        # Ultrasonic
        # ----------------------------------------------------

        if "ultrasonic" in data:

            value = data[
                "ultrasonic"
            ]

            try:

                if isinstance(
                    value,
                    dict
                ):

                    value = value.get(
                        "distance",
                        0
                    )

                latest_sensor_data[
                    "ultrasonic"
                ]["distance"] = float(
                    value
                )

            except (
                ValueError,
                TypeError
            ):

                pass

        # ----------------------------------------------------
        # MQ2
        # ----------------------------------------------------

        if "mq2" in data:

            try:

                latest_sensor_data[
                    "mq2"
                ] = float(
                    data["mq2"]
                )

            except (
                ValueError,
                TypeError
            ):

                pass

        # ----------------------------------------------------
        # PIR
        # ----------------------------------------------------

        if "pir" in data:

            value = data["pir"]

            if isinstance(
                value,
                bool
            ):

                latest_sensor_data[
                    "pir"
                ] = value

            elif isinstance(
                value,
                str
            ):

                latest_sensor_data[
                    "pir"
                ] = (
                    value.lower()
                    in [
                        "true",
                        "1",
                        "yes",
                        "on"
                    ]
                )

            else:

                latest_sensor_data[
                    "pir"
                ] = bool(
                    value
                )

        # ----------------------------------------------------
        # Movement
        # ----------------------------------------------------

        if "movement" in data:

            try:

                latest_sensor_data[
                    "movement"
                ] = int(
                    data["movement"]
                )

            except (
                ValueError,
                TypeError
            ):

                pass

        # ----------------------------------------------------
        # IR
        # ----------------------------------------------------

        if "ir_obstacle" in data:

            value = data[
                "ir_obstacle"
            ]

            if isinstance(
                value,
                bool
            ):

                latest_sensor_data[
                    "ir_obstacle"
                ] = value

            elif isinstance(
                value,
                str
            ):

                latest_sensor_data[
                    "ir_obstacle"
                ] = (
                    value.lower()
                    in [
                        "true",
                        "1",
                        "yes",
                        "on"
                    ]
                )

            else:

                latest_sensor_data[
                    "ir_obstacle"
                ] = bool(
                    value
                )

        # ----------------------------------------------------
        # IR RAW
        # ----------------------------------------------------

        if "ir_raw" in data:

            try:

                latest_sensor_data[
                    "ir_raw"
                ] = int(
                    data["ir_raw"]
                )

            except (
                ValueError,
                TypeError
            ):

                pass

        # ----------------------------------------------------
        # HEARTBEAT
        # ----------------------------------------------------

        last_sensor_time = time.time()
        evaluate_sensor_anomalies()

        return jsonify({
            "status":
                "ok"
        })

    return jsonify(
        latest_sensor_data
    )


# ============================================================
# STATUS
# ============================================================

@app.route("/status")
def status():
    global camera_online

    now = time.time()
    esp32_online = (
        last_sensor_time > 0
        and now - last_sensor_time < ESP32_TIMEOUT
    )

    # A stream is considered online only while recent MJPEG data is arriving.
    if not last_camera_ok or now - last_camera_ok > CAMERA_STATUS_TIMEOUT:
        camera_online = False

    return jsonify({
        "esp32": esp32_online,
        "camera": camera_online,
        "sensor_age": (
            now - last_sensor_time if last_sensor_time > 0 else None
        ),
        "control_age": now - last_control_time,
        "command": latest_command,
        "command_id": command_id,
        "command_age": now - last_command_time
    })


# ============================================================
# CAMERA
# ============================================================

# ============================================================
# CAMERA
# ============================================================

@app.route("/camera")
def camera():
    global camera_online, last_camera_ok

    upstream = None
    try:
        upstream = requests.get(
            f"{ESP32_CAM_URL}/stream",
            stream=True,
            timeout=(3, 15)
        )

        if upstream.status_code != 200:
            upstream.close()
            camera_online = False
            return "ESP32-CAM stream unavailable", 503

        camera_online = True
        last_camera_ok = time.time()
        content_type = upstream.headers.get(
            "Content-Type",
            "multipart/x-mixed-replace; boundary=frame"
        )

        def generate():
            global camera_online, last_camera_ok
            try:
                for chunk in upstream.iter_content(chunk_size=1024):
                    if chunk:
                        last_camera_ok = time.time()
                        camera_online = True
                        yield chunk
            except GeneratorExit:
                raise
            except Exception as error:
                print("[CAMERA STREAM ERROR]", error)
            finally:
                upstream.close()
                camera_online = False

        return Response(
            generate(),
            status=200,
            headers={
                "Content-Type": content_type,
                "Cache-Control": "no-cache, no-store, must-revalidate",
                "Pragma": "no-cache",
                "Expires": "0",
                "X-Accel-Buffering": "no"
            }
        )

    except Exception as error:
        if upstream is not None:
            upstream.close()
        camera_online = False
        print("[CAMERA ERROR]", error)
        return "ESP32-CAM unavailable", 503

# ============================================================
# DEBUG
# ============================================================

@app.route("/debug")
def debug():

    return jsonify({

        "server":
            "KumoGotYou",

        "latest_command":
            latest_command,

        "command_id":
            command_id,

        "latest_control":
            latest_control,

        "latest_sensor_data":
            latest_sensor_data,

        "camera":
            ESP32_CAM_URL,

        "last_sensor_time":
            last_sensor_time
    })


# ============================================================
# AI
# ============================================================

def ask_ai(
    message,
    automatic=False
):

    distance = (
        latest_sensor_data[
            "ultrasonic"
        ]["distance"]
    )

    mq2 = latest_sensor_data[
        "mq2"
    ]

    pir = latest_sensor_data[
        "pir"
    ]

    ir = latest_sensor_data[
        "ir_obstacle"
    ]

    sensor_context = f"""
LIVE KUMOGOTYOU SENSOR DATA

Ultrasonic: {distance:.2f} cm
MQ-2: {mq2}
PIR: {pir}
IR obstacle: {ir}
"""

    system_prompt = f"""
You are Kumo,
the AI intelligence of KumoGotYou,
an ESP32 DevKit hexapod robot.

{sensor_context}

Rules:

- Never invent sensor readings.
- Use the supplied live readings.
- Be concise and natural.
- You are an AI inside a robot.
- Do not claim to be human.
- Do not claim to see something unless
  camera information actually provides it.
- Explain sensor events clearly.
- Not detailed explain. 
"""

    if automatic:

        system_prompt += """
This is an automatic robot alert.
Explain the unusual event using
the supplied sensor readings by using unusual .
"""

    if not OPENROUTER_API_KEY:
        return (None, "OpenRouter API key is not configured. Set OPENROUTER_API_KEY in your environment.")

    headers = {

        "Authorization":
            "Bearer " +
            OPENROUTER_API_KEY,

        "Content-Type":
            "application/json",

        "HTTP-Referer":
            "http://localhost:5000",

        "X-Title":
            "KumoGotYou"
    }

    payload = {

        "model":
            AI_MODEL,

        "messages": [

            {
                "role":
                    "system",

                "content":
                    system_prompt
            },

            {
                "role":
                    "user",

                "content":
                    message
            }
        ]
    }

    try:

        response = requests.post(

            "https://openrouter.ai/api/v1/chat/completions",

            headers=headers,

            json=payload,

            timeout=30
        )

        if response.status_code != 200:

            print(
                "[AI ERROR]",
                response.status_code,
                response.text
            )

            return (
                None,
                "AI API error."
            )

        result = response.json()

        answer = (
            result[
                "choices"
            ][0][
                "message"
            ][
                "content"
            ]
        )

        return (
            answer,
            None
        )

    except Exception as e:

        print(
            "[AI ERROR]",
            e
        )

        return (
            None,
            str(e)
        )


# ============================================================
# AI MANUAL
# ============================================================

@app.route(
    "/ai",
    methods=["POST"]
)
def ai():

    data = request.get_json(
        silent=True
    )

    if not data:

        return jsonify({
            "error":
                "Invalid request."
        }), 400

    message = str(
        data.get(
            "message",
            ""
        )
    ).strip()

    if not message:

        return jsonify({
            "error":
                "Message is empty."
        }), 400

    answer, error = ask_ai(
        message,
        False
    )

    if error:

        return jsonify({
            "error":
                error
        }), 500

    return jsonify({
        "response":
            answer
    })


# ============================================================
# AI ALERT
# ============================================================

# ============================================================
# AI ALERT
# ============================================================

@app.route("/ai_alert", methods=["GET", "POST"])
def ai_alert():
    # GET is polled by the dashboard. POST remains for backward compatibility
    # with firmware that already reports its own alert events.
    if request.method == "GET":
        with _alert_lock:
            return jsonify(dict(current_ai_alert))

    data = request.get_json(silent=True) or {}
    alert_type = str(data.get("type", "ESP32 alert"))[:80]
    reason = str(data.get("reason", "The ESP32 reported an unusual event."))[:400]
    _trigger_local_anomaly(alert_type, reason)
    with _alert_lock:
        return jsonify(dict(current_ai_alert))


# ============================================================
# SERVER
# ============================================================

if __name__ == "__main__":

    print()
    print(
        "======================================"
    )

    print(
        "       KUMOGOTYOU SERVER"
    )

    print(
        "======================================"
    )

    print()

    print(
        "Dashboard:"
    )

    print(
        "http://127.0.0.1:5000"
    )

    print()

    print(
        "Robot command:"
    )

    print(
        "GET/POST /robot_command"
    )

    print()

    print(
        "Joystick:"
    )

    print(
        "GET/POST /control"
    )

    print()

    print(
        "Sensors:"
    )

    print(
        "GET/POST /sensor_data"
    )

    print()

    print(
        "Camera:"
    )

    print(
        "GET /camera"
    )

    print()

    print(
        "Debug:"
    )

    print(
        "GET /debug"
    )

    print()

    app.run(
        host="0.0.0.0",
        port=5000,
        debug=True,
        threaded=True
    )