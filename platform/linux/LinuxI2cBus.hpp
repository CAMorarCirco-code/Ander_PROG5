#pragma once

// Linux (Raspberry Pi) implementation of bme280::Bus on top of the kernel
// i2c-dev interface (/dev/i2c-N). Only this file pair and LinuxClock include
// Linux headers; the core only sees a Bus&.

#include <bme280/Bus.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

namespace bme280 {

/// RAII owner of one /dev/i2c-N file descriptor, bound to one slave address.
///
/// The constructor opens the device and selects the slave address; if either
/// step fails it throws std::system_error, so a LinuxI2cBus that exists is
/// always usable. The destructor closes the descriptor.
///
/// Non-copyable (two owners would close the same fd) and non-movable (a
/// Bme280 keeps a Bus& to it, which a move would leave pointing at an empty
/// shell).
class LinuxI2cBus final : public Bus {
public:
    static constexpr const char* kDefaultDevice = "/dev/i2c-1";   // RPi header pins 3/5

    /// Throws std::system_error if the device cannot be opened or the
    /// address cannot be selected (e.g. EBUSY: a kernel driver owns it).
    explicit LinuxI2cBus(uint8_t address, const std::string& device = kDefaultDevice);
    ~LinuxI2cBus() override;

    LinuxI2cBus(const LinuxI2cBus&)            = delete;
    LinuxI2cBus& operator=(const LinuxI2cBus&) = delete;
    LinuxI2cBus(LinuxI2cBus&&)                 = delete;
    LinuxI2cBus& operator=(LinuxI2cBus&&)      = delete;

    /// Write `reg`, repeated start, read `len` bytes - one I2C_RDWR
    /// transaction with two messages, so there is no STOP in between.
    bool read(uint8_t reg, uint8_t* data, size_t len) override;

    /// Write `reg` followed by `len` bytes in one message.
    bool write(uint8_t reg, const uint8_t* data, size_t len) override;

    uint8_t address() const { return address_; }
    const std::string& device() const { return device_; }

private:
    // The Bosch driver never transfers more than 26 bytes at once (calibration
    // block); keep the same 32-byte limit as ArduinoI2cBus so both platforms
    // accept exactly the same transfers. Also avoids heap use per transfer.
    static constexpr size_t kMaxTransfer = 32;

    std::string device_;
    uint8_t     address_;
    int         fd_ = -1;
};

} // namespace bme280
