
#include "OLED.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_SDA 21
#define OLED_SCL 22

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C
#define OLED_RESET -1

static Adafruit_SSD1306 display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    OLED_RESET
);

bool initOLED() {

    Wire.begin(OLED_SDA, OLED_SCL);
    Wire.setClock(100000);

    if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS
    )) {
        Serial.println("OLED initialization failed!");
        return false;
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    display.setCursor(0, 0);
    display.println("KumoGotYou");
    display.println("OLED Ready");

    display.display();

    return true;
}

void updateOLED(
    const char* status,
    int distanceCm,
    int mq2Value,
    bool pirDetected,
    bool irDetected
) {

    display.clearDisplay();

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // Header
    display.setCursor(0, 0);
    display.println("KumoGotYou");

    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // Status
    display.setCursor(0, 13);
    display.print("Status: ");
    display.println(status);

    // Ultrasonic
    display.setCursor(0, 24);
    display.print("Distance: ");

    if (distanceCm >= 0) {
        display.print(distanceCm);
        display.println(" cm");
    } else {
        display.println("No reading");
    }

    // MQ2
    display.setCursor(0, 35);
    display.print("MQ2: ");
    display.println(mq2Value);

    // PIR
    display.setCursor(0, 46);
    display.print("PIR: ");
    display.println(pirDetected ? "DETECTED" : "CLEAR");

    // IR
    display.setCursor(0, 57);
    display.print("IR: ");
    display.println(irDetected ? "BLOCKED" : "CLEAR");

    display.display();
}

void clearOLED() {
    display.clearDisplay();
    display.display();
}
