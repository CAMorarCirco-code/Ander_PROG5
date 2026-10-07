// Week 5 station layer on the host: FakeSensor, JSON payload, and the
// Sampler's lifecycle and threading contract. No hardware, no broker.
//
// Timing: the tests wait for observable events with generous timeouts
// instead of asserting exact times, so they also pass under valgrind,
// sanitizers and qemu. Only two tests bound durations, both with wide
// margins against what a broken implementation would show.

#include "station/FakeSensor.hpp"
#include "station/Json.hpp"
#include "station/Publisher.hpp"
#include "station/Sampler.hpp"
#include "TestMain.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <type_traits>
#include <vector>

namespace {

using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;
using bme280::Error;
using bme280::Measurement;

static_assert(!std::is_copy_constructible<station::Sampler>::value, "Sampler owns a thread");
static_assert(std::is_constructible<station::Sampler, station::EnvironmentSensor&, std::chrono::milliseconds>::value,
              "Sampler depends on the interface");
static_assert(std::is_abstract<station::EnvironmentSensor>::value && std::is_abstract<station::Publisher>::value,
              "interfaces");

double ms(Clock::duration d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

/// Collects callback invocations; lets the test wait for N of them.
struct Recorder {
    std::mutex                   mutex;
    std::condition_variable      cv;
    std::vector<Measurement>     values;
    std::vector<std::thread::id> threads;
    std::vector<Clock::time_point> times;

    void operator()(const Measurement& m)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            values.push_back(m);
            threads.push_back(std::this_thread::get_id());
            times.push_back(Clock::now());
        }
        cv.notify_all();
    }
    bool waitFor(size_t n, std::chrono::milliseconds timeout = 5000ms)
    {
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_for(lock, timeout, [&] { return values.size() >= n; });
    }
    size_t count()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return values.size();
    }
};

/// Test sensor: configurable read time and failures, and it detects two
/// overlapping readForced() calls (the pitfall "sampler + main").
class ProbeSensor final : public station::EnvironmentSensor {
public:
    std::chrono::milliseconds readTime{0};
    int                       failEvery = 0;   // every n-th read fails (0: never)
    std::atomic<int>          reads{0};
    std::atomic<int>          overlaps{0};
    std::atomic<bool>         inRead{false};

    Error init() override { return Error::None; }

    Error readForced(Measurement& out) override
    {
        if (inRead.exchange(true)) {
            ++overlaps;
        }
        const int n = ++reads;
        if (readTime.count() > 0) {
            std::this_thread::sleep_for(readTime);
        }
        inRead = false;
        if (failEvery > 0 && n % failEvery == 0) {
            return Error::BusFailure;
        }
        out.temperatureC = static_cast<float>(n);
        return Error::None;
    }
};

// --- FakeSensor / JSON -------------------------------------------------------

void fakeSensorIsARamp()
{
    station::FakeSensor sensor;
    Measurement m;
    CHECK(sensor.readForced(m) == Error::NotInitialised);
    CHECK(sensor.init() == Error::None);
    for (int i = 0; i < 52; ++i) {
        CHECK(sensor.readForced(m) == Error::None);
        CHECK_NEAR(m.temperatureC, 20.0 + 0.1 * (i % 50), 1e-4);
        CHECK_NEAR(m.pressurePa, 101325.0, 0.0);
        CHECK_NEAR(m.humidityPct, 45.2, 1e-4);
    }
}

void jsonMatchesAssignmentFormat()
{
    Measurement m;
    m.temperatureC = 21.37F;
    m.pressurePa   = 101325.0F;
    m.humidityPct  = 45.2F;
    CHECK(station::toJson(m) == R"({"t":21.37,"p":101325,"h":45.2})");   // assignment example

    m.temperatureC = -5.5F;
    m.pressurePa   = 99999.6F;
    m.humidityPct  = 0.0F;
    CHECK(station::toJson(m) == R"({"t":-5.50,"p":100000,"h":0.0})");

    m.temperatureC = 20.0F;
    m.pressurePa   = 101325.0F;
    m.humidityPct  = 45.2F;
    CHECK(station::toJson(m) == R"({"t":20.00,"p":101325,"h":45.2})");   // first FakeSensor sample
}

// --- Sampler ----------------------------------------------------------------

void callbackRunsOnSamplerThreadWithRamp()
{
    station::FakeSensor sensor;
    CHECK(sensor.init() == Error::None);
    Recorder rec;
    station::Sampler sampler(sensor, 10ms);
    sampler.onMeasurement([&](const Measurement& m) { rec(m); });
    CHECK(!sampler.isRunning());
    sampler.start();
    CHECK(sampler.isRunning());
    CHECK(rec.waitFor(5));
    sampler.stop();

    std::lock_guard<std::mutex> lock(rec.mutex);
    CHECK(rec.values.size() >= 5);
    for (size_t i = 0; i < rec.values.size(); ++i) {
        CHECK(rec.threads[i] != std::this_thread::get_id());   // not main
        CHECK(rec.threads[i] == rec.threads[0]);               // one sampler thread
        CHECK_NEAR(rec.values[i].temperatureC, 20.0 + 0.1 * static_cast<double>(i % 50), 1e-4);
    }
}

void noCallbackAfterStopReturns()
{
    station::FakeSensor sensor;
    (void)sensor.init();
    Recorder rec;
    station::Sampler sampler(sensor, 5ms);
    sampler.onMeasurement([&](const Measurement& m) { rec(m); });
    sampler.start();
    CHECK(rec.waitFor(3));
    sampler.stop();
    CHECK(!sampler.isRunning());
    const size_t frozen = rec.count();
    std::this_thread::sleep_for(100ms);   // 20 periods
    CHECK(rec.count() == frozen);
}

void destructorStopsAndJoins()
{
    station::FakeSensor sensor;
    (void)sensor.init();
    Recorder rec;
    {
        station::Sampler sampler(sensor, 5ms);
        sampler.onMeasurement([&](const Measurement& m) { rec(m); });
        sampler.start();
        CHECK(rec.waitFor(2));
    }   // ~Sampler: no stop() call by the test
    const size_t frozen = rec.count();
    std::this_thread::sleep_for(100ms);
    CHECK(rec.count() == frozen);
}

void startStopAreIdempotentAndRestartable()
{
    station::FakeSensor sensor;
    (void)sensor.init();
    Recorder rec;
    station::Sampler sampler(sensor, 5ms);
    sampler.stop();                        // stop before start: no-op
    sampler.onMeasurement([&](const Measurement& m) { rec(m); });
    sampler.start();
    sampler.start();                       // second start: no second thread
    CHECK(rec.waitFor(3));
    sampler.stop();
    sampler.stop();                        // second stop: no-op
    const size_t first = rec.count();

    sampler.start();                       // restart after stop
    CHECK(sampler.isRunning());
    CHECK(rec.waitFor(first + 3));
    sampler.stop();

    std::lock_guard<std::mutex> lock(rec.mutex);
    // One sensor, one ramp: no sample lost or duplicated by start/stop.
    for (size_t i = 0; i < rec.values.size(); ++i) {
        CHECK_NEAR(rec.values[i].temperatureC, 20.0 + 0.1 * static_cast<double>(i % 50), 1e-4);
    }
    // Two runs -> two different threads, never two at the same time.
    CHECK(rec.threads.front() != std::this_thread::get_id());
}

void failedReadsSkipCallbackButSamplingContinues()
{
    ProbeSensor sensor;
    sensor.failEvery = 2;                  // reads 2, 4, 6, ... fail
    Recorder rec;
    station::Sampler sampler(sensor, 5ms);
    sampler.onMeasurement([&](const Measurement& m) { rec(m); });
    sampler.start();
    CHECK(rec.waitFor(5));
    sampler.stop();
    std::lock_guard<std::mutex> lock(rec.mutex);
    for (const Measurement& m : rec.values) {
        CHECK(static_cast<int>(m.temperatureC) % 2 == 1);   // only odd reads arrive
    }
    CHECK(sensor.reads >= 9);
}

void sensorIsNeverReadConcurrently()
{
    ProbeSensor sensor;
    sensor.readTime = 2ms;
    Recorder rec;
    station::Sampler sampler(sensor, 1ms);  // period shorter than the read
    sampler.onMeasurement([&](const Measurement& m) { rec(m); });
    sampler.start();
    CHECK(rec.waitFor(20));
    sampler.stop();
    CHECK(sensor.overlaps == 0);
}

void stopDoesNotWaitOutThePeriod()
{
    ProbeSensor sensor;
    Recorder rec;
    station::Sampler sampler(sensor, 10s);
    sampler.onMeasurement([&](const Measurement& m) { rec(m); });
    sampler.start();
    CHECK(rec.waitFor(1));                 // now sleeping until t = 10 s
    const auto t0 = Clock::now();
    sampler.stop();
    CHECK(ms(Clock::now() - t0) < 2000.0); // typically < 1 ms; broken: ~10000
    CHECK(rec.count() == 1);
}

void stopWithSlowSensorReturnsWithinOnePeriod()
{
    // Bonus of step 6: the read in progress (here 300 ms) is allowed to
    // finish, nothing after it is waited for.
    ProbeSensor sensor;
    sensor.readTime = 300ms;
    station::Sampler sampler(sensor, 1s);
    sampler.start();
    while (!sensor.inRead) {
        std::this_thread::yield();
    }
    const auto t0 = Clock::now();
    sampler.stop();
    const double took = ms(Clock::now() - t0);
    CHECK(took < 1000.0);                  // within one period
    CHECK(sensor.reads == 1);              // no second read was started
}

void periodDoesNotDriftWithReadTime()
{
    // Read 30 ms, period 100 ms: deadlines from the start give ~100 ms per
    // sample; "sleep(period) after the read" would give ~130 ms.
    ProbeSensor sensor;
    sensor.readTime = 30ms;
    Recorder rec;
    station::Sampler sampler(sensor, 100ms);
    sampler.onMeasurement([&](const Measurement& m) { rec(m); });
    sampler.start();
    CHECK(rec.waitFor(8, 10000ms));
    sampler.stop();
    std::lock_guard<std::mutex> lock(rec.mutex);
    const double avg = ms(rec.times[7] - rec.times[0]) / 7.0;
    CHECK(avg >= 95.0 && avg < 125.0);
}

void callbackCanBeReplacedWhileRunning()
{
    station::FakeSensor sensor;
    (void)sensor.init();
    Recorder a;
    Recorder b;
    station::Sampler sampler(sensor, 2ms);
    sampler.onMeasurement([&](const Measurement& m) { a(m); });
    sampler.start();
    CHECK(a.waitFor(2));
    sampler.onMeasurement([&](const Measurement& m) { b(m); });   // no data race (TSan)
    CHECK(b.waitFor(2));
    const size_t aFrozen = a.count();
    std::this_thread::sleep_for(20ms);
    sampler.stop();
    CHECK(a.count() == aFrozen);
}

void samplerWithoutCallbackStillSamples()
{
    ProbeSensor sensor;
    station::Sampler sampler(sensor, 2ms);
    sampler.start();
    const auto deadline = Clock::now() + 5s;
    while (sensor.reads < 3 && Clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    sampler.stop();
    CHECK(sensor.reads >= 3);
}

} // namespace

int main()
{
    RUN(fakeSensorIsARamp);
    RUN(jsonMatchesAssignmentFormat);
    RUN(callbackRunsOnSamplerThreadWithRamp);
    RUN(noCallbackAfterStopReturns);
    RUN(destructorStopsAndJoins);
    RUN(startStopAreIdempotentAndRestartable);
    RUN(failedReadsSkipCallbackButSamplingContinues);
    RUN(sensorIsNeverReadConcurrently);
    RUN(stopDoesNotWaitOutThePeriod);
    RUN(stopWithSlowSensorReturnsWithinOnePeriod);
    RUN(periodDoesNotDriftWithReadTime);
    RUN(callbackCanBeReplacedWhileRunning);
    RUN(samplerWithoutCallbackStillSamples);
    return TEST_RESULT();
}
