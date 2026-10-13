#pragma once

// Linux implementation of hal::Clock on top of clock_nanosleep().

#include <hal/Clock.hpp>

namespace platform {

class LinuxClock final : public hal::Clock {
public:
    /// Sleeps until CLOCK_MONOTONIC has advanced by at least `us`, also when
    /// interrupted by a signal (EINTR) - Clock promises "at least".
    void delayUs(uint32_t us) override;
};

} // namespace platform
