#pragma once

#include <Arduino.h>

namespace Sensors {

/**
 * Touch detector class for detecting edges/cliffs
 */
class TouchDetector {
public:
    TouchDetector();

    /**
     * Initialize the cliff detector with a direct GPIO pin
     * @param pin The GPIO pin for the sensor
     * @return true if initialization was successful, false otherwise
     */
    bool init(int pin = 32);

    /**
     * Update sensor readings
     */
    void update();

    /**
     * Check if a cliff is detected on the side
     * @return true if a cliff is detected, false otherwise
     */
    bool detected();

private:
    int _pin;
    bool _detected;
    int _threshold;
    bool _initialized;

    // Helper method to read from either GPIO or I/O extender
    int readPin();
};

} // namespace Sensors
