#include "app/setup.h"

I2SSpeaker *i2sSpeaker;

void setupSpeakers() {
  i2sSpeaker = new I2SSpeaker(
    GPIO_NUM_13,
    GPIO_NUM_15,
    GPIO_NUM_2
  );

  if (!i2sSpeaker->init(16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO) == ESP_OK) {
    Serial.println("I2S speaker initialization failed");
    delete i2sSpeaker;
    i2sSpeaker = nullptr;
  }
}