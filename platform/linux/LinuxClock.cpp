#include "LinuxClock.hpp"

#include <cerrno>
#include <ctime>

namespace bme280 {

void LinuxClock::delayUs(uint32_t us)
{
    // Absolute deadline on the monotonic clock: a signal that interrupts the
    // sleep does not shorten it (we just sleep again until the same deadline),
    // and changing the wall-clock time has no effect.
    timespec deadline{};
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += static_cast<time_t>(us / 1000000U);
    deadline.tv_nsec += static_cast<long>(us % 1000000U) * 1000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec += 1;
        deadline.tv_nsec -= 1000000000L;
    }
    while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, nullptr) == EINTR) {
    }
}

} // namespace bme280
