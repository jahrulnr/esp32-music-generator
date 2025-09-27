#include "app/setup.h"

Utils::FileManager *fileManager;

void setupFilemanager() {
  fileManager = new Utils::FileManager();
  if (!fileManager->init()) {
    Serial.println("FileManager initialization failed");
  }
}