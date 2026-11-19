
#include <Arduino.h>
#include <WiFi.h>

#include "Ultrasonic.h"
#include "OLED.h"
#include "MQ2.h"
#include "PIR.h"
#include "IR.h"
#include "KumoServer.h"
#include "KumoMovement.h"

// ==========================================
// NETWORK CONFIGURATION
// ==========================================

#ifndef KUMO_WIFI_SSID
#define KUMO_WIFI_SSID "YOUR_WIFI_SSID"
#endif
#ifndef KUMO_WIFI_PASSWORD
#define KUMO_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif
static const char* WIFI_SSID = KUMO_WIFI_SSID;
static const char* WIFI_PASSWORD = KUMO_WIFI_PASSWORD;

static const char* SERVER_URL =
    "http://10.15.5.106:5000";

// ==========================================
// FUNCTION DECLARATIONS
// ==========================================

void sensorTask(void *pvParameters);
void networkTask(void *pvParameters);
void handleServerCommand(const String& command);

// ==========================================
// GLOBAL VARIABLES
// ==========================================

bool oledReady = false;

float distanceCm = -1;
int gasValue = 0;
bool motionDetected = false;
bool obstacleDetected = false;

KumoServer kumoServer;
KumoMovement kumoMovement;

static portMUX_TYPE sensorMux =
    portMUX_INITIALIZER_UNLOCKED;

struct SensorSnapshot {
    float distanceCm;
    int gasValue;
    bool motionDetected;
    bool obstacleDetected;
};

// ==========================================
// SETUP
// ==========================================

void setup() {

    Serial.begin(115200);

    Serial.println();
    Serial.println("================================");
    Serial.println("       KumoGotYou System");
    Serial.println("================================");

    // OLED
    oledReady = initOLED();

    if (oledReady) {
        updateOLED("STARTING", -1, 0, false, false);
        Serial.println("[OK] OLED initialized");
    } else {
        Serial.println("[ERROR] OLED initialization failed");
    }

    // PCA9685 servo controllers and movement engine
    // OLED initialization above already starts Wire on GPIO 21/22.
    kumoMovement.begin();

    // Sensors
    initMQ2();
    Serial.println("[OK] MQ-2 initialized");

    initPIR();
    Serial.println("[OK] PIR initialized");

    initIR();
    Serial.println("[OK] IR initialized");

    // Ultrasonic
    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT_PULLDOWN);

    digitalWrite(PIN_TRIG, LOW);

    attachInterrupt(
        digitalPinToInterrupt(PIN_ECHO),
        echoISR,
        CHANGE
    );

    Serial.println("[OK] Ultrasonic initialized");

    // Wi-Fi and Flask
    kumoServer.begin(
        WIFI_SSID,
        WIFI_PASSWORD,
        SERVER_URL
    );

    Serial.println("[NETWORK] Wi-Fi connection started");

    // Sensor task: Core 1
    BaseType_t sensorCreated = xTaskCreatePinnedToCore(
        sensorTask,
        "Sensor_Task",
        4096,
        NULL,
        1,
        NULL,
        1
    );

    if (sensorCreated == pdPASS) {
        Serial.println("[OK] Sensor task created");
    } else {
        Serial.println("[ERROR] Sensor task creation failed");
    }

    // Network task: Core 0
    BaseType_t networkCreated = xTaskCreatePinnedToCore(
        networkTask,
        "Network_Task",
        6144,
        NULL,
        1,
        NULL,
        0
    );

    if (networkCreated == pdPASS) {
        Serial.println("[OK] Network task created");
    } else {
        Serial.println("[ERROR] Network task creation failed");
    }
}

// ==========================================
// MAIN LOOP
// ==========================================

void loop() {
    kumoMovement.update(millis());
    vTaskDelay(pdMS_TO_TICKS(10));
}

// ==========================================
// SENSOR TASK
// ==========================================

void sensorTask(void *pvParameters) {

    (void)pvParameters;

    for (;;) {

        // ----------------------------------
        // 1. ULTRASONIC SENSOR
        // ----------------------------------

        pulseReady = false;

        digitalWrite(PIN_TRIG, LOW);
        delayMicroseconds(2);

        digitalWrite(PIN_TRIG, HIGH);
        delayMicroseconds(10);

        digitalWrite(PIN_TRIG, LOW);

        const uint32_t timeout = millis();

        while (!pulseReady && millis() - timeout < 30) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        float newDistanceCm = -1;

        if (pulseReady) {

            pulseReady = false;

            const float reading = getFilteredDistance();

            newDistanceCm = (reading > 0) ? reading : -1;
        }

        // ----------------------------------
        // 2. MQ-2
        // ----------------------------------

        const int newGasValue = readMQ2();

        // ----------------------------------
        // 3. PIR
        // ----------------------------------

        const bool newMotionDetected = readPIR();

        // ----------------------------------
        // 4. IR
        // ----------------------------------

        const bool newObstacleDetected = readIR();

        // ----------------------------------
        // 5. UPDATE SENSOR SNAPSHOT
        // ----------------------------------

        portENTER_CRITICAL(&sensorMux);

        distanceCm = newDistanceCm;
        gasValue = newGasValue;
        motionDetected = newMotionDetected;
        obstacleDetected = newObstacleDetected;

        portEXIT_CRITICAL(&sensorMux);

        // ----------------------------------
        // 6. SERIAL MONITOR
        // ----------------------------------

        Serial.println("---------- SENSOR DATA ----------");

        Serial.printf(
            "Distance : %.2f cm\n",
            newDistanceCm
        );

        Serial.printf(
            "MQ-2     : %d\n",
            newGasValue
        );

        Serial.printf(
            "PIR      : %s\n",
            newMotionDetected ? "DETECTED" : "CLEAR"
        );

        Serial.printf(
            "IR       : %s\n",
            newObstacleDetected ? "BLOCKED" : "CLEAR"
        );

        // ----------------------------------
        // 7. OLED
        // ----------------------------------

        static uint32_t lastOLEDUpdate = 0;

        if (oledReady && millis() - lastOLEDUpdate >= 250) {

            lastOLEDUpdate = millis();

            updateOLED(
                "RUNNING",
                newDistanceCm >= 0
                    ? (int)(newDistanceCm + 0.5f)
                    : -1,
                newGasValue,
                newMotionDetected,
                newObstacleDetected
            );
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ==========================================
// NETWORK TASK
// ==========================================

void networkTask(void *pvParameters) {

    (void)pvParameters;

    uint32_t lastSensorPost = 0;
    uint32_t lastCommandPoll = 0;
    uint32_t lastWiFiStatus = 0;

    constexpr uint32_t SENSOR_POST_INTERVAL_MS = 1000;
    constexpr uint32_t COMMAND_POLL_INTERVAL_MS = 300;
    constexpr uint32_t WIFI_STATUS_INTERVAL_MS = 2000;

    for (;;) {

        kumoServer.updateWiFi();

        const uint32_t now = millis();

        // ----------------------------------
        // WIFI STATUS
        // ----------------------------------

        if (now - lastWiFiStatus >= WIFI_STATUS_INTERVAL_MS) {

            lastWiFiStatus = now;

            if (kumoServer.isConnected()) {

                Serial.printf(
                    "[NETWORK] WiFi CONNECTED | IP: %s\n",
                    WiFi.localIP().toString().c_str()
                );

                Serial.printf(
                    "[NETWORK] Signal strength: %d dBm\n",
                    WiFi.RSSI()
                );

            } else {

                Serial.printf(
                    "[NETWORK] WiFi DISCONNECTED | Status: %d\n",
                    WiFi.status()
                );
            }
        }

        // ----------------------------------
        // SERVER COMMUNICATION
        // ----------------------------------

        if (kumoServer.isConnected()) {

            // Send sensor data
            if (now - lastSensorPost >= SENSOR_POST_INTERVAL_MS) {

                lastSensorPost = now;

                SensorSnapshot snapshot;

                portENTER_CRITICAL(&sensorMux);

                snapshot.distanceCm = distanceCm;
                snapshot.gasValue = gasValue;
                snapshot.motionDetected = motionDetected;
                snapshot.obstacleDetected = obstacleDetected;

                portEXIT_CRITICAL(&sensorMux);

                Serial.println("[NETWORK] Posting sensor data...");

                const bool sent = kumoServer.sendSensorData(
                    snapshot.distanceCm,
                    snapshot.gasValue,
                    snapshot.motionDetected,
                    snapshot.obstacleDetected
                );

                Serial.println(
                    sent
                        ? "[SERVER] Sensor data sent successfully"
                        : "[SERVER] Sensor POST failed"
                );
            }

            // Poll robot commands
            if (now - lastCommandPoll >= COMMAND_POLL_INTERVAL_MS) {

                lastCommandPoll = now;

                String command;

                if (kumoServer.pollCommand(command)) {
                    handleServerCommand(command);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ==========================================
// SERVER COMMAND HANDLER
// ==========================================

void handleServerCommand(const String& command) {

    Serial.printf(
        "[SERVER COMMAND] %s\n",
        command.c_str()
    );

    // The network task only updates the requested motion.
    // Servo/I2C writes happen in loop() to keep them out of HTTP polling.
    kumoMovement.command(command);
}
