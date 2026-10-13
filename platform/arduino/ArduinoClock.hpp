#pragma once

// Arduino implementation of hal::Clock on top of delay()/delayMicroseconds().

#include "../../include/hal/Clock.hpp"

namespace platform {

class ArduinoClock final : public hal::Clock {
public:
    void delayUs(uint32_t us) override;
};

} // namespace platform
