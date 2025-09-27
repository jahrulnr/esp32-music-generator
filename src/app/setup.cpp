#include "setup.h"
#include "SendTask.h"

int powerPin = 12;

void setupApp(){
  pinMode(powerPin, OUTPUT);
  analogWrite(powerPin, 4096);

	setupNotification();
	setupDisplay();
	setupMicrophone();
	setupSpeakers();
	setupTouch();

	setupFilemanager();
	setupWiFi();
	setupFTPServer();
	setupWeather();
}

void setupTasksCpu0(){
	startLofiStreaming();
}

void setupTasksCpu1(){
	SendTask::createLoopTaskOnCore([&](void *param){
		long sleepInterval = 30*1000;
		long lastSleep = millis();
		bool sleepMode = false;
		display->enableMutex();

		long apiUpdateInterval = 30000;
		long apiLastUpdate = millis();

		long lastTouch = 0;
		long touchHoldCounter = 0;
		long touchHoldTreshold = 1000;

		long slowUpdateInterval = 500;
		long updateInterval = 50;
		TickType_t lastWakeTime = xTaskGetTickCount();
		while(1){
			xTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(sleepMode ? slowUpdateInterval : updateInterval));

			ftpSrv.handleFTP();
			touchSensor->update();

			if (sleepMode && touchSensor->detected()) {
				lastSleep = millis();
				sleepMode = false;
			  analogWrite(powerPin, 4096);
				Serial.println("touch detected");
			} else if (!sleepMode && millis() - lastSleep > sleepInterval) {
				sleepMode = true;
				display->clear();
			  analogWrite(powerPin, 2048);
			}

			if (sleepMode) {
				continue;
			}
			
			if (lastTouch == 0 && touchSensor->detected()) {
				lastTouch = millis();
			} else if (!touchSensor->detected()){
				lastTouch = 0;
				touchHoldCounter = 0;
			}

			if (lastTouch > 0 && millis() - lastTouch > touchHoldTreshold && touchHoldCounter == 0) {
				Serial.println("touch holded");
				if (isLofiStreaming()) 
					stopLofiStreaming();
				else 
					startLofiStreaming();
				touchHoldCounter++;
			} 
			
			display->update();
			display->setMicLevel(
				microphone->readLevel()
			);

			if (millis() - apiLastUpdate > apiUpdateInterval && weatherService) {
				SendTask::createTaskOnCore([&](void){
					weatherHandler();
					vTaskDeleteWithCaps(NULL);
				}, "weatherUpdate", 4096, 0, 0);
				apiLastUpdate = millis();
			}
		}
	}, "ServiceUpdater", 4096, 5, 1);
}