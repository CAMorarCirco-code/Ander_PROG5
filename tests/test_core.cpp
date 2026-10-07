// Host tests for the platform-independent core: Bme280 + Bosch driver behind
// a MockBus and a FakeClock. Nothing here knows about Arduino or Linux.

#include <bme280/Bme280.hpp>

#include "FakeBme280.hpp"
#include "TestMain.hpp"

#include <type_traits>
#include <utility>
#include <vector>

namespace {

// --- Compile-time interface checks ---------------------------------------

// Bus must not carry timing any more (ISP), and vice versa.
template <typename T, typename = void>
struct HasDelayUs : std::false_type {};
template <typename T>
struct HasDelayUs<T, std::void_t<decltype(std::declval<T&>().delayUs(0U))>> : std::true_type {};

template <typename T, typename = void>
struct HasRead : std::false_type {};
template <typename T>
struct HasRead<T, std::void_t<decltype(std::declval<T&>().read(uint8_t{}, nullptr, size_t{}))>>
    : std::true_type {};

static_assert(!HasDelayUs<bme280::Bus>::value, "Bus must not provide timing");
static_assert(HasRead<bme280::Bus>::value, "Bus provides read()");
static_assert(HasDelayUs<bme280::Clock>::value, "Clock provides delayUs()");
static_assert(!HasRead<bme280::Clock>::value, "Clock must not provide bus access");
static_assert(std::has_virtual_destructor<bme280::Bus>::value, "Bus needs a virtual destructor");
static_assert(std::has_virtual_destructor<bme280::Clock>::value, "Clock needs a virtual destructor");
static_assert(std::is_abstract<bme280::Bus>::value && std::is_abstract<bme280::Clock>::value, "interfaces");
static_assert(std::is_constructible<bme280::Bme280, bme280::Bus&, bme280::Clock&>::value,
              "Bme280(Bus&, Clock&)");
static_assert(!std::is_constructible<bme280::Bme280, bme280::Bus&>::value, "a Clock is required");
static_assert(!std::is_copy_constructible<bme280::Bme280>::value, "Bme280 is non-copyable");
static_assert(!std::is_move_constructible<bme280::Bme280>::value, "Bme280 is non-movable");
// Week 5: Bme280 implements the interface the Sampler depends on (DIP).
static_assert(std::is_base_of<station::EnvironmentSensor, bme280::Bme280>::value,
              "Bme280 is an EnvironmentSensor");
static_assert(std::has_virtual_destructor<station::EnvironmentSensor>::value, "interface");

// --- Test doubles ---------------------------------------------------------

class MockBus final : public bme280::Bus {
public:
    test::FakeBme280 chip;
    bool             fail = false;
    unsigned         reads = 0;
    unsigned         writes = 0;

    bool read(uint8_t reg, uint8_t* data, size_t len) override
    {
        ++reads;
        if (fail) {
            return false;
        }
        chip.read(reg, data, len);
        return true;
    }

    bool write(uint8_t reg, const uint8_t* data, size_t len) override
    {
        ++writes;
        if (fail) {
            return false;
        }
        chip.write(reg, data, len);
        return true;
    }
};

class FakeClock final : public bme280::Clock {
public:
    std::vector<uint32_t> delays;
    void delayUs(uint32_t us) override { delays.push_back(us); }
};

// --- Tests ----------------------------------------------------------------

void initSucceedsAndUsesClockForStartupDelay()
{
    MockBus   bus;
    FakeClock clock;
    bme280::Bme280 sensor(bus, clock);

    CHECK(!sensor.isInitialised());
    CHECK(sensor.init() == bme280::Error::None);
    CHECK(sensor.isInitialised());
    CHECK(bus.chip.softResets == 1);
    // Soft reset waits the 2 ms datasheet startup time - through the Clock.
    CHECK(clock.delays.size() == 1);
    CHECK(!clock.delays.empty() && clock.delays[0] == 2000U);
}

void readForcedWaitsMeasurementTimeOnClock()
{
    MockBus   bus;
    FakeClock clock;
    bme280::Bme280 sensor(bus, clock);
    CHECK(sensor.init() == bme280::Error::None);
    clock.delays.clear();

    bme280::Measurement m;
    CHECK(sensor.readForced(m) == bme280::Error::None);
    CHECK(bus.chip.forcedTriggers == 1);
    CHECK(clock.delays.size() == 1);
    CHECK(!clock.delays.empty() && clock.delays[0] == sensor.measurementTimeUs());
    CHECK(sensor.measurementTimeUs() > 0U);

    // Datasheet example values.
    CHECK_NEAR(m.temperatureC, 25.08, 0.005);
    CHECK_NEAR(m.pressurePa, 100653.0, 2.0);
    CHECK(m.humidityPct >= 0.0F && m.humidityPct <= 100.0F);
}

void configureWritesOversampling()
{
    MockBus   bus;
    FakeClock clock;
    bme280::Bme280 sensor(bus, clock);
    bme280::Config cfg;
    cfg.temperature = bme280::Oversampling::x2;
    cfg.pressure    = bme280::Oversampling::x16;
    cfg.humidity    = bme280::Oversampling::x4;
    cfg.filter      = bme280::Filter::Coeff4;
    CHECK(sensor.init(cfg) == bme280::Error::None);
    CHECK((bus.chip.regs[0xF2] & 0x07) == 0x03);              // osrs_h = x4
    CHECK(((bus.chip.regs[0xF4] >> 5) & 0x07) == 0x02);       // osrs_t = x2
    CHECK(((bus.chip.regs[0xF4] >> 2) & 0x07) == 0x05);       // osrs_p = x16
    CHECK(((bus.chip.regs[0xF5] >> 2) & 0x07) == 0x02);       // filter = 4
}

void wrongChipIdIsReported()
{
    MockBus   bus;
    FakeClock clock;
    bus.chip.chipId = 0x58;   // BMP280
    bme280::Bme280 sensor(bus, clock);
    CHECK(sensor.init() == bme280::Error::WrongChipId);
    CHECK(!sensor.isInitialised());
}

void busFailureIsReported()
{
    MockBus   bus;
    FakeClock clock;
    bus.fail = true;
    bme280::Bme280 sensor(bus, clock);
    CHECK(sensor.init() == bme280::Error::BusFailure);

    bus.fail = false;
    CHECK(sensor.init() == bme280::Error::None);
    bus.fail = true;
    bme280::Measurement m;
    CHECK(sensor.read(m) == bme280::Error::BusFailure);
    CHECK(sensor.readForced(m) == bme280::Error::BusFailure);
}

void callsBeforeInitAreRejectedWithoutTouchingHardware()
{
    MockBus   bus;
    FakeClock clock;
    bme280::Bme280 sensor(bus, clock);
    bme280::Measurement m;
    CHECK(sensor.read(m) == bme280::Error::NotInitialised);
    CHECK(sensor.readForced(m) == bme280::Error::NotInitialised);
    CHECK(sensor.setMode(bme280::Mode::Normal) == bme280::Error::NotInitialised);
    CHECK(sensor.configure(bme280::Config{}) == bme280::Error::NotInitialised);
    CHECK(bus.reads == 0 && bus.writes == 0);
    CHECK(clock.delays.empty());
}

void worksThroughEnvironmentSensorInterface()
{
    // What the Sampler sees: only init() and readForced(), via the interface.
    MockBus   bus;
    FakeClock clock;
    bme280::Bme280             concrete(bus, clock);
    station::EnvironmentSensor& sensor = concrete;
    bme280::Measurement m;
    CHECK(sensor.readForced(m) == bme280::Error::NotInitialised);
    CHECK(sensor.init() == bme280::Error::None);
    CHECK(sensor.readForced(m) == bme280::Error::None);
    CHECK_NEAR(m.temperatureC, 25.08, 0.005);

    // The Week 3/4 overload with a Config is still there.
    bme280::Config cfg;
    cfg.pressure = bme280::Oversampling::x16;
    CHECK(concrete.init(cfg) == bme280::Error::None);
    CHECK(((bus.chip.regs[0xF4] >> 2) & 0x07) == 0x05);
}

void twoSensorsShareOneClock()
{
    // The point of the split: one Clock, any number of buses/sensors.
    MockBus   busA;
    MockBus   busB;
    FakeClock clock;
    bme280::Bme280 a(busA, clock);
    bme280::Bme280 b(busB, clock);
    CHECK(a.init() == bme280::Error::None);
    CHECK(b.init() == bme280::Error::None);
    CHECK(clock.delays.size() == 2);
    CHECK(busA.chip.softResets == 1 && busB.chip.softResets == 1);
}

} // namespace

int main()
{
    RUN(initSucceedsAndUsesClockForStartupDelay);
    RUN(readForcedWaitsMeasurementTimeOnClock);
    RUN(configureWritesOversampling);
    RUN(wrongChipIdIsReported);
    RUN(busFailureIsReported);
    RUN(callsBeforeInitAreRejectedWithoutTouchingHardware);
    RUN(worksThroughEnvironmentSensorInterface);
    RUN(twoSensorsShareOneClock);
    return TEST_RESULT();
}
