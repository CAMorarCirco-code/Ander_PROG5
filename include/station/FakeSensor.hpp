#pragma once

// A sensor that needs no hardware: temperature ramps from the base value
// (default 20.0 C) in 0.1 C steps over 50 samples, then wraps; pressure and
// humidity are constant. Lets the
// whole sampling + MQTT chain run on a laptop.
// LSP: substitutable for any EnvironmentSensor.

#include "../hal/EnvironmentSensor.hpp"

namespace station {

class FakeSensor final : public hal::EnvironmentSensor {
public:
    /// `baseTemperatureC` lets a station run two fakes that are told apart
    /// in the output (week 6: one message per sensor).
    explicit FakeSensor(float baseTemperatureC = 20.0F) : baseTemperatureC_(baseTemperatureC) {}

    hal::Error init() override
    {
        initialised_ = true;
        return hal::Error::None;
    }

    hal::Error readForced(hal::Measurement& out) override
    {
        if (!initialised_) {
            return hal::Error::NotInitialised;
        }
        out.temperatureC = baseTemperatureC_ + static_cast<float>(tick_ % 50) * 0.1F;
        out.pressurePa   = 101325.0F;
        out.humidityPct  = 45.2F;
        ++tick_;
        return hal::Error::None;
    }

private:
    float    baseTemperatureC_;
    bool     initialised_ = false;
    unsigned tick_        = 0;
};

} // namespace station
