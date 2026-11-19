
#include <Arduino.h>
#include <WiFi.h>
#include "esp_camera.h"
#include "esp_http_server.h"

// ==========================================
// WIFI CONFIGURATION
// ==========================================

const char* WIFI_SSID = "REDMI K80";
const char* WIFI_PASSWORD = "QwEr@123novasoe";

// ==========================================
// AI-THINKER ESP32-CAM PIN CONFIGURATION
// ==========================================

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ==========================================
// HTTP SERVERS
// ==========================================

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;

bool cameraReady = false;

static const char* STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=frame";

static const char* STREAM_BOUNDARY =
    "\r\n--frame\r\n";

static const char* STREAM_PART =
    "Content-Type: image/jpeg\r\n"
    "Content-Length: %u\r\n\r\n";

// ==========================================
// CAMERA INITIALIZATION
// ==========================================


bool initCamera() {

    // Initialize every configuration field to zero
    camera_config_t config = {};

    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;

    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;

    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;

    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;

    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;

    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    config.sccb_i2c_port = 0;

    if (psramFound()) {
        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 12;
        config.fb_count = 2;
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
        config.frame_size = FRAMESIZE_QVGA;
        config.jpeg_quality = 15;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    }

    const esp_err_t result = esp_camera_init(&config);

    if (result != ESP_OK) {
        Serial.printf(
            "[CAMERA] Initialization failed: 0x%x\n",
            result
        );
        return false;
    }

    sensor_t* sensor = esp_camera_sensor_get();

    if (sensor != nullptr) {
        Serial.printf(
            "[CAMERA] Detected sensor PID: 0x%04X\n",
            sensor->id.PID
        );

        sensor->set_framesize(sensor, FRAMESIZE_VGA);
    }

    Serial.println("[CAMERA] Sensor initialized successfully");

    return true;
}


// ==========================================
// CAPTURE ENDPOINT
// GET /capture
// ==========================================

static esp_err_t captureHandler(httpd_req_t* req) {

    camera_fb_t* fb = esp_camera_fb_get();

    if (!fb) {

        Serial.println("[CAMERA] Capture failed");

        httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Camera capture failed"
        );

        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");

    httpd_resp_set_hdr(
        req,
        "Cache-Control",
        "no-store"
    );

    const esp_err_t result = httpd_resp_send(
        req,
        reinterpret_cast<const char*>(fb->buf),
        fb->len
    );

    esp_camera_fb_return(fb);

    return result;
}

// ==========================================
// STREAM ENDPOINT
// GET /stream
// ==========================================

static esp_err_t streamHandler(httpd_req_t* req) {

    camera_fb_t* fb = nullptr;

    char partBuffer[64];

    esp_err_t result = ESP_OK;

    result = httpd_resp_set_type(
        req,
        STREAM_CONTENT_TYPE
    );

    if (result != ESP_OK) {
        return result;
    }

    httpd_resp_set_hdr(
        req,
        "Access-Control-Allow-Origin",
        "*"
    );

    while (true) {

        fb = esp_camera_fb_get();

        if (!fb) {

            Serial.println("[CAMERA] Stream capture failed");

            result = ESP_FAIL;
            break;
        }

        result = httpd_resp_send_chunk(
            req,
            STREAM_BOUNDARY,
            strlen(STREAM_BOUNDARY)
        );

        if (result != ESP_OK) {
            esp_camera_fb_return(fb);
            break;
        }

        const size_t headerLength = snprintf(
            partBuffer,
            sizeof(partBuffer),
            STREAM_PART,
            static_cast<unsigned int>(fb->len)
        );

        result = httpd_resp_send_chunk(
            req,
            partBuffer,
            headerLength
        );

        if (result == ESP_OK) {

            result = httpd_resp_send_chunk(
                req,
                reinterpret_cast<const char*>(fb->buf),
                fb->len
            );
        }

        esp_camera_fb_return(fb);
        fb = nullptr;

        if (result != ESP_OK) {
            break;
        }
    }

    return result;
}

// ==========================================
// START HTTP SERVERS
// ==========================================

void startCameraServer() {

    // Capture server: port 80
    httpd_config_t captureConfig = HTTPD_DEFAULT_CONFIG();

    captureConfig.server_port = 80;
    captureConfig.ctrl_port = 32768;
    captureConfig.max_uri_handlers = 4;

    httpd_uri_t captureUri = {
        .uri = "/capture",
        .method = HTTP_GET,
        .handler = captureHandler,
        .user_ctx = NULL
    };

    if (httpd_start(&camera_httpd, &captureConfig) == ESP_OK) {

        httpd_register_uri_handler(
            camera_httpd,
            &captureUri
        );

        Serial.println("[HTTP] Capture endpoint started");
    } else {

        Serial.println("[HTTP] Capture server failed");
    }

    // Streaming server: port 81
    httpd_config_t streamConfig = HTTPD_DEFAULT_CONFIG();

    streamConfig.server_port = 81;
    streamConfig.ctrl_port = 32769;
    streamConfig.max_uri_handlers = 4;

    httpd_uri_t streamUri = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = streamHandler,
        .user_ctx = NULL
    };

    if (httpd_start(&stream_httpd, &streamConfig) == ESP_OK) {

        httpd_register_uri_handler(
            stream_httpd,
            &streamUri
        );

        Serial.println("[HTTP] Stream endpoint started");
    } else {

        Serial.println("[HTTP] Stream server failed");
    }
}

// ==========================================
// WIFI CONNECTION
// ==========================================

void connectWiFi() {

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);

    Serial.println("[WiFi] Connecting to hotspot...");

    WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
    );

    const unsigned long startTime = millis();

    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - startTime < 20000
    ) {

        delay(500);
        Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {

        Serial.println("[WiFi] Connected");

        Serial.print("[WiFi] IP address: ");
        Serial.println(WiFi.localIP());

        Serial.print("[WiFi] RSSI: ");
        Serial.println(WiFi.RSSI());

        startCameraServer();

    } else {

        Serial.printf(
            "[WiFi] Connection failed. Status: %d\n",
            WiFi.status()
        );
    }
}

// ==========================================
// SETUP
// ==========================================

void setup() {

    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("================================");
    Serial.println("   KumoGotYou Camera System");
    Serial.println("================================");

    cameraReady = initCamera();

    if (!cameraReady) {

        Serial.println("[ERROR] Camera initialization failed");

        while (true) {
            delay(1000);
        }
    }

    connectWiFi();
}

// ==========================================
// MAIN LOOP
// ==========================================

void loop() {

    if (
        WiFi.status() != WL_CONNECTED
    ) {

        Serial.println("[WiFi] Disconnected. Retrying...");

        WiFi.disconnect();
        WiFi.begin(
            WIFI_SSID,
            WIFI_PASSWORD
        );

        delay(10000);

        return;
    }

    delay(1000);
}
