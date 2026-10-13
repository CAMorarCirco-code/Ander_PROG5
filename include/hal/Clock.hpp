#pragma once

#include <cstdint>

namespace hal {

/// Blocking delay the sensor needs between commands and results. Kept apart
/// from Bus (ISP): a bus does not have to know about time, and a clock can be
/// shared by several drivers on different buses.
/// Part of the hal package (week 7): shared by all sensors, depends on nothing.
/// Implementations: platform::ArduinoClock, platform::LinuxClock, FakeClock (tests).
class Clock {
public:
    virtual ~Clock() = default;

    /// Block for at least `us` microseconds.
    virtual void delayUs(uint32_t us) = 0;

protected:
    Clock()                        = default;
    Clock(const Clock&)            = default;
    Clock& operator=(const Clock&) = default;
};

} // namespace hal
