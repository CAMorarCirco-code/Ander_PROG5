#pragma once

// What the station sees of a sensor. Nothing about I2C, Bosch or BME280.
// DIP: Sampler depends on this, never on bme280::Bme280 directly;
// bme280::Bme280 implements it.
//
// Part of the core package (week 6): the core owns its interfaces, so the
// core has no dependency on the app layer. Relative include, like the other
// core headers, so it also compiles in the Arduino IDE.

#include "Types.hpp"

namespace bme280 {

class EnvironmentSensor {
public:
    virtual ~EnvironmentSensor() = default;

    /// Bring the sensor to a state in which readForced() works.
    virtual Error init() = 0;

    /// Take one measurement, block until it is available.
    virtual Error readForced(Measurement& out) = 0;

protected:
    // Interfaces are used through references/pointers. Protected copy
    // operations stop slicing ("*a = *b" through the base) while still
    // letting implementations be copyable if they want to (same as Bus/Clock).
    EnvironmentSensor()                            = default;
    EnvironmentSensor(const EnvironmentSensor&)            = default;
    EnvironmentSensor& operator=(const EnvironmentSensor&) = default;
};

} // namespace bme280
