#pragma once

// Arduino implementation of bme280::Bus on top of Wire (TwoWire).
// Together with ArduinoI2cBus.cpp this is the only library code that includes
// <Wire.h> / <Arduino.h>; the core only sees a Bus&.

#include "../../include/bme280/Bus.hpp"

#include <Arduino.h>
#include <Wire.h>

namespace bme280 {

class ArduinoI2cBus final : public Bus {
public:
    /// `wire` must already be started (Wire.begin()) by its owner, the sketch.
    ArduinoI2cBus(TwoWire& wire, uint8_t address);

    /// Write `reg`, repeated start, read `len` bytes.
    bool read(uint8_t reg, uint8_t* data, size_t len) override;

    /// Write `reg` followed by `len` bytes in one transmission.
    bool write(uint8_t reg, const uint8_t* data, size_t len) override;

    void delayUs(uint32_t us) override;

private:
    // Smallest Wire buffer across cores (AVR: 32 bytes). The Bosch driver
    // never transfers more than 26 bytes at once.
    static constexpr size_t kMaxTransfer = 32;

    TwoWire& wire_;
    uint8_t  address_;
};

} // namespace bme280
