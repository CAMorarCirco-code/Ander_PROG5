#pragma once

// Linux implementation of bme280::Clock on top of clock_nanosleep().

#include <bme280/Clock.hpp>

namespace bme280 {

class LinuxClock final : public Clock {
public:
    /// Sleeps until CLOCK_MONOTONIC has advanced by at least `us`, also when
    /// interrupted by a signal (EINTR) - Clock promises "at least".
    void delayUs(uint32_t us) override;
};

} // namespace bme280
