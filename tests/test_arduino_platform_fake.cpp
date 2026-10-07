// ArduinoI2cBus and ArduinoClock compiled on the host against a recording
// fake Wire/Arduino (tests/fake_arduino). Checks the Bus contract: argument
// validation, repeated-start reads, exactly `len` bytes, register + data
// writes, and the delay split of ArduinoClock.

#include "ArduinoClock.hpp"
#include "ArduinoI2cBus.hpp"
#include "TestMain.hpp"

#include <string>
#include <vector>

namespace {

std::vector<std::string> g_delays;

} // namespace

void delay(unsigned long ms) { g_delays.push_back("delay " + std::to_string(ms)); }
void delayMicroseconds(unsigned int us) { g_delays.push_back("delayMicroseconds " + std::to_string(us)); }

namespace {

using Calls = std::vector<std::string>;

void readIsRepeatedStartThenRequest()
{
    TwoWire wire;
    wire.rxData = {0x60, 0x11, 0x22};
    bme280::ArduinoI2cBus bus(wire, 0x76);
    uint8_t buf[3] = {};
    CHECK(bus.read(0xD0, buf, 3));
    CHECK((wire.calls == Calls{"begin 76", "end stop=0", "request 76 3"}));
    CHECK(wire.sent.size() == 1 && wire.sent[0] == std::vector<uint8_t>{0xD0});
    CHECK(buf[0] == 0x60 && buf[1] == 0x11 && buf[2] == 0x22);
}

void readRejectsInvalidArgumentsWithoutBusTraffic()
{
    TwoWire wire;
    bme280::ArduinoI2cBus bus(wire, 0x76);
    uint8_t buf[64] = {};
    CHECK(!bus.read(0xD0, nullptr, 1));    // the Week 4 review fix
    CHECK(!bus.read(0xD0, nullptr, 0));
    CHECK(!bus.read(0xD0, buf, 0));
    CHECK(!bus.read(0xD0, buf, 33));
    CHECK(wire.calls.empty());
    wire.rxData.assign(32, 0xAB);
    CHECK(bus.read(0x88, buf, 32));        // upper limit still accepted
}

void readFailsOnNackOrShortRead()
{
    TwoWire wire;
    wire.rxData = {1, 2, 3, 4};
    bme280::ArduinoI2cBus bus(wire, 0x76);
    uint8_t buf[4] = {9, 9, 9, 9};

    wire.endResult = 2;                    // NACK on address
    CHECK(!bus.read(0xF7, buf, 4));
    CHECK((wire.calls == Calls{"begin 76", "end stop=0"}));   // no requestFrom

    wire.endResult = 0;
    wire.shortBy   = 1;                    // device delivered only 3 of 4
    wire.calls.clear();
    CHECK(!bus.read(0xF7, buf, 4));
    CHECK(buf[0] == 9);                    // nothing copied on failure
}

void writeIsRegisterThenDataInOneTransmission()
{
    TwoWire wire;
    bme280::ArduinoI2cBus bus(wire, 0x77);
    const uint8_t data[] = {0x05, 0xF4, 0x25};
    CHECK(bus.write(0xF2, data, sizeof data));
    CHECK((wire.calls == Calls{"begin 77", "end stop=1"}));
    CHECK(wire.sent.size() == 1 && (wire.sent[0] == std::vector<uint8_t>{0xF2, 0x05, 0xF4, 0x25}));

    wire.endResult = 3;                    // NACK on data
    CHECK(!bus.write(0xF2, data, sizeof data));
}

void writeRejectsInvalidArgumentsWithoutBusTraffic()
{
    TwoWire wire;
    bme280::ArduinoI2cBus bus(wire, 0x76);
    uint8_t buf[32] = {};
    CHECK(!bus.write(0xF4, nullptr, 1));   // consistent with LinuxI2cBus
    CHECK(!bus.write(0xF4, buf, 32));      // 32 + register byte > 32
    CHECK(wire.calls.empty());
    CHECK(bus.write(0xF4, buf, 31));       // exactly 32 on the wire
    CHECK(bus.write(0xF4, nullptr, 0));    // register pointer only
    CHECK(wire.sent.size() == 2 && wire.sent[1] == std::vector<uint8_t>{0xF4});
}

void clockSplitsMillisAndMicros()
{
    bme280::ArduinoClock clock;
    g_delays.clear();
    clock.delayUs(9300);
    clock.delayUs(2000);
    clock.delayUs(999);
    CHECK((g_delays == Calls{"delay 9", "delayMicroseconds 300", "delay 2", "delayMicroseconds 0", "delay 0",
                             "delayMicroseconds 999"}));
}

} // namespace

int main()
{
    RUN(readIsRepeatedStartThenRequest);
    RUN(readRejectsInvalidArgumentsWithoutBusTraffic);
    RUN(readFailsOnNackOrShortRead);
    RUN(writeIsRegisterThenDataInOneTransmission);
    RUN(writeRejectsInvalidArgumentsWithoutBusTraffic);
    RUN(clockSplitsMillisAndMicros);
    return TEST_RESULT();
}
