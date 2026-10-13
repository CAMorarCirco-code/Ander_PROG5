#pragma once

#include "../hal/Types.hpp"

#include <cstdint>

// BME280-specific types. The shared ones (Error, Measurement) live in hal;
// they are re-exported here so users of the sensor can keep writing
// bme280::Error / bme280::Measurement.

namespace bme280 {

using hal::Error;
using hal::errorName;
using hal::Measurement;

/// I2C address depends on the SDO pin level.
enum class I2cAddress : uint8_t { Low = 0x76, High = 0x77 };

enum class Oversampling : uint8_t { Skip, x1, x2, x4, x8, x16 };
enum class Filter : uint8_t { Off, Coeff2, Coeff4, Coeff8, Coeff16 };
enum class Standby : uint8_t { ms0_5, ms62_5, ms125, ms250, ms500, ms1000, ms10, ms20 };
enum class Mode : uint8_t { Sleep, Forced, Normal };

/// Full sensor configuration. Defaults follow the datasheet
/// "weather monitoring" use case (forced mode, x1/x1/x1, filter off).
struct Config {
    Oversampling temperature = Oversampling::x1;
    Oversampling pressure    = Oversampling::x1;
    Oversampling humidity    = Oversampling::x1;
    Filter       filter      = Filter::Off;
    Standby      standby     = Standby::ms1000;  ///< only used in Mode::Normal
};

} // namespace bme280
