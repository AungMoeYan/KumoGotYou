#include "Ultrasonic.h"

const int PIN_TRIG = 12;
const int PIN_ECHO = 13;
const size_t FILTER_SIZE = 5;

volatile uint32_t echoStart = 0;
volatile uint32_t echoEnd = 0;
volatile bool pulseReady = false;

// Interrupt Service Routine
void IRAM_ATTR echoISR() {
  uint32_t now = esp_timer_get_time();
  if (digitalRead(PIN_ECHO) == HIGH) {
    echoStart = now;
  } else {
    echoEnd = now;
    pulseReady = true;
  }
}

// Signal Filtering Logic
float getFilteredDistance() {
  static float readings[5] = {0}; // FILTER_SIZE အတိုင်း
  static size_t index = 0;

  uint32_t duration = echoEnd - echoStart;
  float rawDistance = (float)duration / 58.2;

  if (rawDistance < 2.0 || rawDistance > 400.0) return -1.0; 

  readings[index] = rawDistance;
  index = (index + 1) % FILTER_SIZE;

  float sorted[5];
  memcpy(sorted, readings, sizeof(readings));
  
  for (size_t i = 0; i < FILTER_SIZE - 1; i++) {
    for (size_t j = i + 1; j < FILTER_SIZE; j++) {
      if (sorted[i] > sorted[j]) {
        float temp = sorted[i];
        sorted[i] = sorted[j];
        sorted[j] = temp;
      }
    }
  }
  return sorted[FILTER_SIZE / 2];
}
