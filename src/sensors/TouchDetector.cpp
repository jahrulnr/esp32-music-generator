#include "TouchDetector.h"

namespace Sensors {

TouchDetector::TouchDetector() : _pin(-1),
                                 _detected(false),
                                 _initialized(false) {
}

bool TouchDetector::init(int pin) {
    _pin = pin;

    pinMode(_pin, INPUT);

    _initialized = true;
    return true;
}

void TouchDetector::update() {
    if (!_initialized) {
        return;
    }
    // Use digitalRead: 1 means cliff detected
    _detected = readPin();
}

bool TouchDetector::detected() {
    return _detected;
}

int TouchDetector::readPin() {
    if (!_initialized) {
        return LOW;
    }
    
    return digitalRead(_pin);
}

} // namespace Sensors
