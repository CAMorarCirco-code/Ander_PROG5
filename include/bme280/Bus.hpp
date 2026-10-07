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

    /// Read `len` bytes starting at register `reg` as one transaction:
    /// register pointer, repeated start (no STOP), `len` data bytes.
    /// Returns true only if exactly `len` bytes were stored in `data`.
    /// Returns false without bus traffic if `data` is null, `len` is 0 or
    /// `len` exceeds the implementation's transfer limit (32 on both
    /// ArduinoI2cBus and LinuxI2cBus).
    virtual bool read(uint8_t reg, uint8_t* data, size_t len) = 0;

    /// Write `reg` followed by the `len` bytes of `data` in one transaction.
    /// Returns true only if the device acknowledged all of it.
    /// Returns false without bus traffic if `data` is null while `len` > 0,
    /// or `len` + 1 exceeds the transfer limit.
    virtual bool write(uint8_t reg, const uint8_t* data, size_t len) = 0;

protected:
    Bus()                      = default;
    Bus(const Bus&)            = default;
    Bus& operator=(const Bus&) = default;
};

} // namespace bme280
