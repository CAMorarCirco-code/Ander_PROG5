// LinuxI2cBus and LinuxClock against the real kernel/libc (no fakes):
// error paths that need no I2C hardware, descriptor ownership, and timing.

#include "FdCount.hpp"
#include "LinuxClock.hpp"
#include "LinuxI2cBus.hpp"
#include "TestMain.hpp"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <ctime>
#include <system_error>
#include <type_traits>

#include <sys/time.h>

namespace {

static_assert(!std::is_copy_constructible<bme280::LinuxI2cBus>::value, "fd has one owner");
static_assert(!std::is_copy_assignable<bme280::LinuxI2cBus>::value, "fd has one owner");
static_assert(!std::is_move_constructible<bme280::LinuxI2cBus>::value, "Bme280 holds a Bus&");
static_assert(std::is_base_of<bme280::Bus, bme280::LinuxI2cBus>::value, "LinuxI2cBus is a Bus");
static_assert(std::is_base_of<bme280::Clock, bme280::LinuxClock>::value, "LinuxClock is a Clock");
static_assert(!std::is_base_of<bme280::Clock, bme280::LinuxI2cBus>::value, "bus is not a clock (ISP)");

int errnoOfConstruction(const char* device)
{
    try {
        bme280::LinuxI2cBus bus(0x76, device);
    } catch (const std::system_error& e) {
        return e.code().value();
    }
    return 0;
}

void missingDeviceThrowsEnoent()
{
    const int before = test::openFdCount();
    CHECK(errnoOfConstruction("/dev/i2c-does-not-exist") == ENOENT);
    CHECK(test::openFdCount() == before);
}

void nonI2cDeviceThrowsAndClosesFd()
{
    // /dev/null opens fine but is not an i2c-dev node: I2C_SLAVE -> ENOTTY.
    const int before = test::openFdCount();
    CHECK(errnoOfConstruction("/dev/null") == ENOTTY);
    CHECK(test::openFdCount() == before);
}

void repeatedFailuresDoNotLeak()
{
    const int before = test::openFdCount();
    for (int i = 0; i < 1000; ++i) {
        (void)errnoOfConstruction("/dev/null");
    }
    CHECK(test::openFdCount() == before);
}

double elapsedUs(bme280::Clock& clock, uint32_t us)
{
    const auto t0 = std::chrono::steady_clock::now();
    clock.delayUs(us);
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::micro>(t1 - t0).count();
}

void clockWaitsAtLeastRequested()
{
    bme280::LinuxClock clock;
    for (uint32_t us : {0U, 1U, 500U, 2000U, 9300U, 1500000U}) {
        const double got = elapsedUs(clock, us);
        CHECK(got >= static_cast<double>(us));
        CHECK(got < static_cast<double>(us) + 200000.0);   // generous: shared CI hosts
    }
}

void onAlarm(int) {}

void clockSurvivesSignals()
{
    // SIGALRM every 2 ms while sleeping 50 ms: each signal interrupts
    // clock_nanosleep with EINTR, the delay must still be >= 50 ms.
    struct sigaction sa {};
    sa.sa_handler = onAlarm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;   // no SA_RESTART: we want EINTR
    sigaction(SIGALRM, &sa, nullptr);
    itimerval timer{};
    timer.it_interval.tv_usec = 2000;
    timer.it_value.tv_usec    = 2000;
    setitimer(ITIMER_REAL, &timer, nullptr);

    bme280::LinuxClock clock;
    const double got = elapsedUs(clock, 50000U);

    timer = itimerval{};
    setitimer(ITIMER_REAL, &timer, nullptr);
    signal(SIGALRM, SIG_DFL);
    CHECK(got >= 50000.0);
}

} // namespace

int main()
{
    RUN(missingDeviceThrowsEnoent);
    RUN(nonI2cDeviceThrowsAndClosesFd);
    RUN(repeatedFailuresDoNotLeak);
    RUN(clockWaitsAtLeastRequested);
    RUN(clockSurvivesSignals);
    return TEST_RESULT();
}
