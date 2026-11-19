
#pragma once

#include <Arduino.h>
#include <WiFi.h>

class KumoServer {
public:
    void begin(
        const char* wifiSsid,
        const char* wifiPassword,
        const char* serverBaseUrl
    );

    void updateWiFi();

    bool isConnected() const;
    bool isServerOnline() const;

    bool sendSensorData(
        float distanceCm,
        int mq2Raw,
        bool pirDetected,
        bool irObstacle,
        int irRaw = -1
    );

    bool pollCommand(String& commandOut);

private:
    String _serverBaseUrl;
    String _ssid;
    String _password;

    unsigned long _lastWiFiAttempt = 0;
    unsigned long _lastCommandId = 0;

    bool _started = false;
    bool _connectionStarted = false;
    bool _serverOnline = false;

    String endpoint(const char* path) const;
};
