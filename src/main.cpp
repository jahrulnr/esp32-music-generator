#include <Arduino.h>
#include <esp_task_wdt.h>
#include <LittleFS.h>
#include "app/setup.h"

void setup() {
  Serial.begin(115200);
  LittleFS.begin(true);

  esp_err_t esp_task_wdt_reconfigure(const esp_task_wdt_config_t *config);
  esp_task_wdt_config_t config = {
    .timeout_ms = 120 * 1000,
    .trigger_panic = false,
  };
  esp_task_wdt_reconfigure(&config);

  setupApp();

  for (int freq = 1000; freq >= 200; freq -= 50) {
    i2sSpeaker->playTone(freq, 100, 0.3f);
    delay(50);
  }

  setupTasksCpu0();
  setupTasksCpu1();
}

void loop() {
  vTaskDelete(NULL);
}