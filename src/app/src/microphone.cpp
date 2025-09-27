#include "app/setup.h"

AnalogMicrophone* microphone = nullptr;

void setupMicrophone(){
 	if (!microphone) {
      microphone = new AnalogMicrophone(26, -1, -1);

      bool ret = microphone->init();
      if (!ret) {
          Serial.printf("ERROR: Failed to start analog microphone: %s\n", esp_err_to_name(ret));
          return;
      }
      // microphone->setGain(LOW);
      // microphone->setAttackRelease(true);
  }
}