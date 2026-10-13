#pragma once

// A sensor that needs no hardware: temperature ramps from the base value
// (default 20.0 C) in 0.1 C steps over 50 samples, then wraps; pressure and
// humidity are constant. Lets the
// whole sampling + MQTT chain run on a laptop.
// LSP: substitutable for any EnvironmentSensor.

#include "../bme280/EnvironmentSensor.hpp"

namespace station {

class FakeSensor final : public bme280::EnvironmentSensor {
public:
    /// `baseTemperatureC` lets a station run two fakes that are told apart
    /// in the output (week 6: one message per sensor).
    explicit FakeSensor(float baseTemperatureC = 20.0F) : baseTemperatureC_(baseTemperatureC) {}

    bme280::Error init() override
    {
        initialised_ = true;
        return bme280::Error::None;
    }

    bme280::Error readForced(bme280::Measurement& out) override
    {
        if (!initialised_) {
            return bme280::Error::NotInitialised;
        }
        out.temperatureC = baseTemperatureC_ + static_cast<float>(tick_ % 50) * 0.1F;
        out.pressurePa   = 101325.0F;
        out.humidityPct  = 45.2F;
        ++tick_;
        return bme280::Error::None;
    }

private:
    float    baseTemperatureC_;
    bool     initialised_ = false;
    unsigned tick_        = 0;
};

} // namespace station
