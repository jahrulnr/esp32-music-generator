#include "app/setup.h"

Sensors::TouchDetector* touchSensor = nullptr;

void setupTouch(){
	touchSensor = new Sensors::TouchDetector();
	touchSensor->init(33);
}