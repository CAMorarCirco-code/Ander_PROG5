#pragma once

#include <cstddef>
#include <cstdint>

namespace bme280 {

/// Register-level byte transport the sensor needs (DIP: the driver depends on
/// this abstraction, never on Wire/HAL/Linux directly). Timing is not part of
/// it, see Clock.hpp (ISP).
/// Implementations: ArduinoI2cBus (platform/arduino), LinuxI2cBus
/// (platform/linux), MockBus (tests).
class Bus {
public:
    virtual ~Bus() = default;

    /// Read `len` bytes starting at register `reg`. Return true on success.
    virtual bool read(uint8_t reg, uint8_t* data, size_t len) = 0;

    /// Write `len` bytes starting at register `reg`. Return true on success.
    virtual bool write(uint8_t reg, const uint8_t* data, size_t len) = 0;

protected:
    Bus()                      = default;
    Bus(const Bus&)            = default;
    Bus& operator=(const Bus&) = default;
};

} // namespace bme280
