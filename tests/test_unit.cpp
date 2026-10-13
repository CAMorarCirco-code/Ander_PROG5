// Week 7, assignment 7: unit tests on the host with doctest, a MockBus
// (256-byte register map + write log) and a FakeClock that only counts.
// No mocking framework; the Bus/Clock split is what makes this possible.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "FakeClock.hpp"
#include "MockBus.hpp"
#include "RecordingPublisher.hpp"

#include <bme280/Bme280.hpp>
#include <station/FakeSensor.hpp>
#include <station/Json.hpp>
#include <station/Sampler.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>

using bme280::Bme280;
using bme280::Error;
using bme280::Measurement;
using test::FakeClock;
using test::MockBus;

namespace {

// --- Datasheet worked example (Bosch BMP280/BME280 compensation example) ---
// Calibration and raw ADC values printed in the datasheet, and the results
// printed there: T = 25.08 degC, p = 100653.27 Pa.
constexpr uint16_t kT1 = 27504;
constexpr int16_t  kT2 = 26435;
constexpr int16_t  kT3 = -1000;
constexpr uint16_t kP1 = 36477;
constexpr int16_t  kP2 = -10685;
constexpr int16_t  kP3 = 3024;
constexpr int16_t  kP4 = 2855;
constexpr int16_t  kP5 = 140;
constexpr int16_t  kP6 = -7;
constexpr int16_t  kP7 = 15500;
constexpr int16_t  kP8 = -14600;
constexpr int16_t  kP9 = 6000;
constexpr int32_t  kAdcT = 519888;
constexpr int32_t  kAdcP = 415148;

// The BME280 datasheet prints no humidity example. These typical humidity
// coefficients are checked against the datasheet's double-precision formula
// below (H4/H5 share a nibble-packed register, a classic source of bugs).
constexpr uint8_t kH1 = 75;
constexpr int16_t kH2 = 362;
constexpr uint8_t kH3 = 0;
constexpr int16_t kH4 = 313;
constexpr int16_t kH5 = 50;
constexpr int8_t  kH6 = 30;
constexpr int32_t kAdcH = 30000;

void put20(MockBus& bus, uint8_t reg, int32_t adc)
{
    bus.reg(reg)                            = static_cast<uint8_t>(adc >> 12);
    bus.reg(static_cast<uint8_t>(reg + 1)) = static_cast<uint8_t>((adc >> 4) & 0xFF);
    bus.reg(static_cast<uint8_t>(reg + 2)) = static_cast<uint8_t>((adc & 0x0F) << 4);
}

/// Pre-load the calibration block (0x88..0xA1, 0xE1..0xE7) and the raw data
/// registers (0xF7..0xFE) exactly as the chip lays them out.
void loadDatasheetExample(MockBus& bus)
{
    bus.put16(0x88, kT1);
    bus.put16(0x8A, static_cast<uint16_t>(kT2));
    bus.put16(0x8C, static_cast<uint16_t>(kT3));
    bus.put16(0x8E, kP1);
    bus.put16(0x90, static_cast<uint16_t>(kP2));
    bus.put16(0x92, static_cast<uint16_t>(kP3));
    bus.put16(0x94, static_cast<uint16_t>(kP4));
    bus.put16(0x96, static_cast<uint16_t>(kP5));
    bus.put16(0x98, static_cast<uint16_t>(kP6));
    bus.put16(0x9A, static_cast<uint16_t>(kP7));
    bus.put16(0x9C, static_cast<uint16_t>(kP8));
    bus.put16(0x9E, static_cast<uint16_t>(kP9));
    bus.reg(0xA1) = kH1;
    bus.put16(0xE1, static_cast<uint16_t>(kH2));
    bus.reg(0xE3) = kH3;
    bus.reg(0xE4) = static_cast<uint8_t>(kH4 >> 4);
    bus.reg(0xE5) = static_cast<uint8_t>((kH4 & 0x0F) | ((kH5 & 0x0F) << 4));
    bus.reg(0xE6) = static_cast<uint8_t>(kH5 >> 4);
    bus.reg(0xE7) = static_cast<uint8_t>(kH6);
    put20(bus, 0xF7, kAdcP);
    put20(bus, 0xFA, kAdcT);
    bus.reg(0xFD) = static_cast<uint8_t>(kAdcH >> 8);
    bus.reg(0xFE) = static_cast<uint8_t>(kAdcH & 0xFF);
}

/// Datasheet double-precision formulas (independent of the Bosch C code).
double referenceTFine()
{
    const double var1 = (kAdcT / 16384.0 - kT1 / 1024.0) * kT2;
    const double d    = kAdcT / 131072.0 - kT1 / 8192.0;
    const double var2 = d * d * kT3;
    return var1 + var2;
}

double referenceHumidity()
{
    double h = referenceTFine() - 76800.0;
    h = (kAdcH - (kH4 * 64.0 + kH5 / 16384.0 * h)) *
        (kH2 / 65536.0 * (1.0 + kH6 / 67108864.0 * h * (1.0 + kH3 / 67108864.0 * h)));
    h = h * (1.0 - kH1 * h / 524288.0);
    return h < 0.0 ? 0.0 : (h > 100.0 ? 100.0 : h);
}

} // namespace

// 1.
TEST_CASE("init() returns WrongChipId when register 0xD0 is not 0x60")
{
    MockBus   bus;
    FakeClock clock;
    bus.reg(MockBus::kRegChipId) = 0x58;   // a BMP280
    Bme280 sensor(bus, clock);

    CHECK(sensor.init() == Error::WrongChipId);
    CHECK_FALSE(sensor.isInitialised());
}

// 2.
TEST_CASE("init() returns BusFailure when the bus fails the first read")
{
    MockBus   bus;
    FakeClock clock;
    bus.failNextRead();
    Bme280 sensor(bus, clock);

    CHECK(sensor.init() == Error::BusFailure);
    CHECK(bus.reads() == 1);               // it gave up at the chip-ID read
    CHECK_FALSE(sensor.isInitialised());
}

// 3.
TEST_CASE("read() before init() returns NotInitialised")
{
    MockBus   bus;
    FakeClock clock;
    Bme280    sensor(bus, clock);
    Measurement m;

    CHECK(sensor.read(m) == Error::NotInitialised);
    CHECK(sensor.readForced(m) == Error::NotInitialised);
    CHECK(bus.reads() == 0);               // no bus traffic before init()
}

// 4.
TEST_CASE("init() succeeds with the correct chip ID and the calibration block pre-loaded")
{
    MockBus   bus;
    FakeClock clock;
    loadDatasheetExample(bus);
    Bme280 sensor(bus, clock);

    CHECK(sensor.init() == Error::None);
    CHECK(sensor.isInitialised());
    // Soft reset was issued and the 2 ms start-up time went through the Clock.
    bool softReset = false;
    for (const auto& w : bus.writes()) {
        softReset = softReset || (w.reg == 0xE0 && w.value == 0xB6);
    }
    CHECK(softReset);
    CHECK(clock.longest() >= 2000U);
    // Default Config (weather monitoring): x1 / x1 / x1, filter off.
    CHECK((bus.reg(0xF2) & 0x07) == 0x01);
    CHECK(((bus.reg(0xF4) >> 5) & 0x07) == 0x01);
    CHECK(((bus.reg(0xF4) >> 2) & 0x07) == 0x01);
    CHECK(((bus.reg(0xF5) >> 2) & 0x07) == 0x00);
}

// 5.
TEST_CASE("readForced() asks the Clock for at least the measurement time of the current config")
{
    MockBus   bus;
    FakeClock clock;
    loadDatasheetExample(bus);
    Bme280 sensor(bus, clock);

    bme280::Config config;                // a slower config: x16 everywhere
    config.temperature = bme280::Oversampling::x16;
    config.pressure    = bme280::Oversampling::x16;
    config.humidity    = bme280::Oversampling::x16;
    REQUIRE(sensor.init(config) == Error::None);
    clock.clear();

    Measurement m;
    REQUIRE(sensor.readForced(m) == Error::None);
    const uint32_t needed = sensor.measurementTimeUs();
    CHECK(needed > 9300U);                // longer than the x1 default
    CHECK(clock.longest() >= needed);
}

// 6.
TEST_CASE("compensation matches the datasheet's worked example")
{
    MockBus   bus;
    FakeClock clock;
    loadDatasheetExample(bus);
    Bme280 sensor(bus, clock);
    REQUIRE(sensor.init() == Error::None);

    Measurement m;
    REQUIRE(sensor.readForced(m) == Error::None);
    // Printed in the datasheet: T = 25.08 degC, p = 100653.27 Pa. The library
    // uses the 32-bit integer path (0.01 degC, 1 Pa resolution).
    CHECK(m.temperatureC == doctest::Approx(25.08).epsilon(0.0001));
    CHECK(m.pressurePa == doctest::Approx(100653.27).epsilon(0.00002));   // +/- 2 Pa
    // Humidity: datasheet formula (double) as the reference; the integer path
    // has 1/1024 %RH resolution.
    CHECK(m.humidityPct == doctest::Approx(referenceHumidity()).epsilon(0.002));
    CHECK(m.humidityPct > 0.0F);
    CHECK(m.humidityPct < 100.0F);
}

// 7. MqttPublisher is not unit-tested: it needs a running broker, so it is
// covered by an integration test (test_mqtt_publisher starts a real
// mosquitto). Unit-tested instead: Sampler + FakeSensor + RecordingPublisher.
TEST_CASE("Sampler with FakeSensor publishes one message per sensor through a RecordingPublisher")
{
    station::FakeSensor first(20.0F);
    station::FakeSensor second(30.0F);
    REQUIRE(first.init() == Error::None);
    REQUIRE(second.init() == Error::None);
    test::RecordingPublisher publisher;
    const std::vector<std::string> topics{"han/ese/ander/bme280/state", "han/ese/ander/fake/state"};

    station::Sampler sampler({&first, &second}, std::chrono::milliseconds(5));
    sampler.onMeasurement([&](std::size_t i, const Measurement& meas) {
        publisher.publish(topics[i], station::toJson(meas));
    });
    sampler.start();
    REQUIRE(publisher.waitFor(4, std::chrono::milliseconds(5000)));
    sampler.stop();

    const auto msgs = publisher.messages();
    REQUIRE(msgs.size() >= 4);
    CHECK(msgs[0].first == topics[0]);
    CHECK(msgs[0].second == R"({"t":20.00,"p":101325,"h":45.2})");
    CHECK(msgs[1].first == topics[1]);
    CHECK(msgs[1].second == R"({"t":30.00,"p":101325,"h":45.2})");
    CHECK(msgs[2].second == R"({"t":20.10,"p":101325,"h":45.2})");
    CHECK(msgs[3].second == R"({"t":30.10,"p":101325,"h":45.2})");
}
