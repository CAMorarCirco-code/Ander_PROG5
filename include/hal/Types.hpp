#pragma once

// Types shared by every sensor and every user of a sensor (week 7: the
// abstractions moved out of the BME280 package). Measurement is shared on
// purpose: every EnvironmentSensor reports temperature, pressure and
// humidity; a sensor without one of them would need its own design
// decision (see docs/refactor_report.md).

#include <cstdint>

namespace hal {

/// Every public method returns one of these. No exceptions, no negative ints.
enum class Error : uint8_t {
    None = 0,
    NotInitialised,   ///< read() called before init()
    BusFailure,       ///< Bus::read/write returned false
    WrongChipId,      ///< the chip answered with the wrong ID
    InvalidConfig,    ///< the driver rejected the settings
    Unknown           ///< a driver result that has no better mapping
};

/// Name of an Error, for log output. Returns a string literal: no
/// std::string, no allocation, so it is fine on an MCU.
inline const char* errorName(Error e)
{
    switch (e) {
        case Error::None:           return "None";
        case Error::NotInitialised: return "NotInitialised";
        case Error::BusFailure:     return "BusFailure";
        case Error::WrongChipId:    return "WrongChipId";
        case Error::InvalidConfig:  return "InvalidConfig";
        case Error::Unknown:        return "Unknown";
    }
    return "Unknown";
}

/// The hand-off to the rest of the weather station (MQTT payload, DB).
/// Units are part of the name so they cannot be misread.
struct Measurement {
    float temperatureC = 0.0F;
    float pressurePa   = 0.0F;
    float humidityPct  = 0.0F;
};

} // namespace hal
