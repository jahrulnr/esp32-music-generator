#include <Arduino.h>
#include "app/setup.h"

Display::Display* display = nullptr;

void setupDisplay() {
  display = new Display::Display();
#if 1
  if (display->init(27, 14, 128, 64)) {
    display->clear();
    display->drawCenteredText(20, "Cozmo System");
    display->drawCenteredText(40, "Starting...");
    display->update();
  }
#endif
}