#include "ArduinoClock.hpp"

#include <Arduino.h>

namespace platform {

void ArduinoClock::delayUs(uint32_t us)
{
    // delayMicroseconds() is only accurate up to ~16 ms on AVR.
    delay(us / 1000U);
    delayMicroseconds(static_cast<unsigned int>(us % 1000U));
}

} // namespace platform
