
#include "KumoServer.h"

#include <WiFiClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>

namespace {
    constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 10000;
    constexpr uint16_t HTTP_TIMEOUT_MS = 2000;
}

// ==========================================
// INITIALIZATION
// ==========================================
void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {

    switch (event) {

        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("[WiFi EVENT] Connected to access point");
            break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.print("[WiFi EVENT] IP acquired: ");
            Serial.println(WiFi.localIP());
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.printf(
                "[WiFi EVENT] Disconnected. Reason code: %d\n",
                info.wifi_sta_disconnected.reason
            );
            break;

        default:
            break;
    }
}

void KumoServer::begin(
    const char* wifiSsid,
    const char* wifiPassword,
    const char* serverBaseUrl
) {
    WiFi.mode(WIFI_STA);
    WiFi.onEvent(WiFiEvent);
    _ssid = wifiSsid ? wifiSsid : "";
    _password = wifiPassword ? wifiPassword : "";
    _serverBaseUrl = serverBaseUrl ? serverBaseUrl : "";

    _serverBaseUrl.trim();

    while (_serverBaseUrl.endsWith("/")) {
        _serverBaseUrl.remove(_serverBaseUrl.length() - 1);
    }

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);

    Serial.println("\n[WiFi] Scanning nearby networks...");

    int count = WiFi.scanNetworks();

    bool found = false;

    if (count > 0) {
        for (int i = 0; i < count; i++) {

            Serial.printf(
                "[SCAN] %s | RSSI: %d | Channel: %d | Encryption: %d\n",
                WiFi.SSID(i).c_str(),
                WiFi.RSSI(i),
                WiFi.channel(i),
                WiFi.encryptionType(i)
            );

            if (WiFi.SSID(i) == _ssid) {
                found = true;

                Serial.println("[SCAN] TARGET NETWORK FOUND");
            }
        }
    } else {
        Serial.println("[SCAN] No networks found");
    }

    if (!found) {
        Serial.println("[SCAN] TARGET NETWORK NOT FOUND");
    }

    WiFi.scanDelete();

    _started = true;
    _connectionStarted = false;
    _serverOnline = false;

    _lastWiFiAttempt = millis() - WIFI_RETRY_INTERVAL_MS;

    updateWiFi();
}


// ==========================================
// CONNECTION STATUS
// ==========================================

bool KumoServer::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

bool KumoServer::isServerOnline() const {
    return _serverOnline;
}

// ==========================================
// WIFI CONNECTION AND RECONNECTION
// ==========================================

void KumoServer::updateWiFi() {

    if (!_started || isConnected()) {
        return;
    }

    static bool connecting = false;

    if (!connecting) {
        Serial.println("[WiFi] Starting connection...");

        WiFi.begin(
            _ssid.c_str(),
            _password.c_str()
        );

        connecting = true;
    }
}

// ==========================================
// URL BUILDER
// ==========================================

String KumoServer::endpoint(const char* path) const {
    return _serverBaseUrl + path;
}

// ==========================================
// SEND SENSOR DATA
// ==========================================

bool KumoServer::sendSensorData(
    float distanceCm,
    int mq2Raw,
    bool pirDetected,
    bool irObstacle,
    int irRaw
) {

    if (!isConnected()) {
        Serial.println("[SERVER] WiFi disconnected");
        _serverOnline = false;
        return false;
    }

    if (_serverBaseUrl.isEmpty()) {
        Serial.println("[SERVER] Server URL is empty");
        _serverOnline = false;
        return false;
    }

    WiFiClient client;
    HTTPClient http;

    http.setTimeout(HTTP_TIMEOUT_MS);

    const String url = endpoint("/sensor_data");

    Serial.println("[SERVER] Sending sensor data...");
    Serial.println("[SERVER] URL: " + url);

    if (!http.begin(client, url)) {
        Serial.println("[SERVER] HTTP begin failed");
        _serverOnline = false;
        return false;
    }

    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;

    doc["ultrasonic"]["distance"] = distanceCm;
    doc["mq2"] = mq2Raw;
    doc["pir"] = pirDetected;
    doc["ir_obstacle"] = irObstacle;

    if (irRaw >= 0) {
        doc["ir_raw"] = irRaw;
    }

    String body;
    serializeJson(doc, body);

    Serial.println("[SERVER] Payload:");
    Serial.println(body);

    const int statusCode = http.POST(body);

    Serial.printf(
        "[SERVER] HTTP status: %d\n",
        statusCode
    );

    if (statusCode > 0) {

        Serial.println("[SERVER] Response:");
        Serial.println(http.getString());

    } else {

        Serial.println("[SERVER] Error:");
        Serial.println(http.errorToString(statusCode));
    }

    http.end();

    _serverOnline = (
        statusCode >= 200 &&
        statusCode < 300
    );

    return _serverOnline;
}

// ==========================================
// RECEIVE ROBOT COMMAND
// ==========================================

bool KumoServer::pollCommand(String& commandOut) {

    if (!isConnected() || _serverBaseUrl.isEmpty()) {
        return false;
    }

    WiFiClient client;
    HTTPClient http;

    http.setTimeout(HTTP_TIMEOUT_MS);

    const String url = endpoint("/robot_command");

    if (!http.begin(client, url)) {
        Serial.println("[SERVER] Command HTTP begin failed");
        return false;
    }

    const int statusCode = http.GET();

    if (statusCode != HTTP_CODE_OK) {

        Serial.printf(
            "[SERVER] Command HTTP status: %d\n",
            statusCode
        );

        http.end();
        return false;
    }

    const String response = http.getString();

    http.end();

    JsonDocument doc;

    const DeserializationError error =
        deserializeJson(doc, response);

    if (error) {
        Serial.print("[SERVER] Command JSON error: ");
        Serial.println(error.c_str());
        return false;
    }

    const unsigned long commandId =
        doc["id"] | 0UL;

    const char* command =
        doc["command"] | "stop";

    if (commandId == 0 || commandId == _lastCommandId) {
        return false;
    }

    _lastCommandId = commandId;
    commandOut = String(command);

    Serial.print("[SERVER] New command: ");
    Serial.println(commandOut);

    return true;
}
