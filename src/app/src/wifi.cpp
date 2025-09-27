#include <Arduino.h>
#include "app/setup.h"

Services::WiFiService *wifiService;

void setupWiFi() {
  wifiService = new Services::WiFiService(nullptr);
  wifiService->init();

  // Get config (already loaded from file or defaults in constructor)
  Services::WiFiService::WiFiConfig config = wifiService->getConfig();

  if (display) {
    display->clear();
    display->drawCenteredText(20, "Connecting to");
    display->drawCenteredText(40, config.ssid.c_str());
    display->update();
  }

  // Try to connect to WiFi using saved configuration
  if (wifiService->connect(config.ssid, config.password, 10000)) {
    Serial.printf("Connected to WiFi: %s\n", config.ssid.c_str());
    Serial.printf("IP: %s\n", wifiService->getIP().c_str());

    if (display) {
      display->clear();
      display->drawCenteredText(10, "WiFi Connected");
      display->drawCenteredText(30, config.ssid.c_str());
      display->drawCenteredText(50, wifiService->getIP().c_str());
      display->update();
      delay(2000);
    }
  }
}