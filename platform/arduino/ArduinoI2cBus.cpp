#include "ArduinoI2cBus.hpp"

#include <Arduino.h>

namespace bme280 {

ArduinoI2cBus::ArduinoI2cBus(TwoWire& wire, uint8_t address) : wire_(wire), address_(address) {}

bool ArduinoI2cBus::read(uint8_t reg, uint8_t* data, size_t len)
{
    if (len == 0 || len > kMaxTransfer) {
        return false;
    }
    wire_.beginTransmission(address_);
    wire_.write(reg);
    if (wire_.endTransmission(false) != 0) {   // false: repeated start, no STOP
        return false;
    }
    const auto count = static_cast<uint8_t>(len);
    if (wire_.requestFrom(address_, count) != count) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        data[i] = static_cast<uint8_t>(wire_.read());
    }
    return true;
}

bool ArduinoI2cBus::write(uint8_t reg, const uint8_t* data, size_t len)
{
    // The Bosch driver already interleaves further register/value pairs
    // into `data`, so one transmission is all it takes.
    if (len + 1 > kMaxTransfer) {
        return false;
    }
    wire_.beginTransmission(address_);
    wire_.write(reg);
    if (wire_.write(data, len) != len) {
        wire_.endTransmission();
        return false;
    }
    return wire_.endTransmission() == 0;
}

void ArduinoI2cBus::delayUs(uint32_t us)
{
    // delayMicroseconds() is only accurate up to ~16 ms on AVR.
    delay(us / 1000U);
    delayMicroseconds(static_cast<unsigned int>(us % 1000U));
}

} // namespace bme280
