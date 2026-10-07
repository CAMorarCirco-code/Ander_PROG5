#pragma once

// Arduino implementation of bme280::Clock on top of delay()/delayMicroseconds().

#include "../../include/bme280/Clock.hpp"

namespace bme280 {

class ArduinoClock final : public Clock {
public:
    void delayUs(uint32_t us) override;
};

} // namespace bme280
