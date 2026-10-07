// Raspberry Pi smoke test: the unchanged BME280 core on Linux i2c-dev.
//
//   bme280_rpi_smoke [device] [address] [interval_ms] [count]
//   bme280_rpi_smoke /dev/i2c-1 0x76 1000 10     (these are the defaults;
//                                                 count 0 = until Ctrl+C)
//
// Exit code: 0 = every read succeeded, 1 = setup/init failed, 2 = a read failed.

#include <bme280/Bme280.hpp>

#include "LinuxClock.hpp"
#include "LinuxI2cBus.hpp"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>

namespace {

volatile std::sig_atomic_t g_stop = 0;

void onSignal(int) { g_stop = 1; }

// LinuxClock deliberately sleeps through signals (Clock promises "at least"),
// so wait in short slices to react to Ctrl+C within ~100 ms.
void waitMs(bme280::Clock& clock, unsigned long ms)
{
    for (unsigned long waited = 0; waited < ms && g_stop == 0; waited += 100) {
        const unsigned long slice = (ms - waited < 100) ? (ms - waited) : 100;
        clock.delayUs(static_cast<uint32_t>(slice * 1000UL));
    }
}

const char* toString(bme280::Error e)
{
    switch (e) {
        case bme280::Error::None:           return "None";
        case bme280::Error::NotInitialised: return "NotInitialised";
        case bme280::Error::BusFailure:     return "BusFailure";
        case bme280::Error::WrongChipId:    return "WrongChipId";
        case bme280::Error::InvalidConfig:  return "InvalidConfig";
        case bme280::Error::Unknown:        return "Unknown";
    }
    return "Unknown";
}

bool parseUnsigned(const char* text, unsigned long max, unsigned long& out)
{
    char* end = nullptr;
    const unsigned long value = std::strtoul(text, &end, 0);   // base 0: accepts 0x76
    if (end == text || *end != '\0' || value > max) {
        return false;
    }
    out = value;
    return true;
}

int usage(const char* argv0)
{
    std::fprintf(stderr, "usage: %s [device=/dev/i2c-1] [address=0x76|0x77] [interval_ms=1000] [count=10, 0=forever]\n",
                 argv0);
    return 1;
}

} // namespace

int main(int argc, char** argv)
{
    std::string   device   = bme280::LinuxI2cBus::kDefaultDevice;
    unsigned long address  = static_cast<unsigned long>(bme280::I2cAddress::Low);
    unsigned long interval = 1000;
    unsigned long count    = 10;

    if (argc > 5) {
        return usage(argv[0]);
    }
    if (argc > 1) {
        device = argv[1];
    }
    if ((argc > 2 && !parseUnsigned(argv[2], 0x7F, address)) ||
        (argc > 3 && !parseUnsigned(argv[3], 3600000UL, interval)) ||
        (argc > 4 && !parseUnsigned(argv[4], 1000000UL, count))) {
        return usage(argv[0]);
    }

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    try {
        // Owners first, user last: bus and clock outlive the sensor because
        // they are declared before it (destroyed in reverse order).
        bme280::LinuxI2cBus bus(static_cast<uint8_t>(address), device);
        bme280::LinuxClock  clock;
        bme280::Bme280      sensor(bus, clock);

        const bme280::Error err = sensor.init();
        if (err != bme280::Error::None) {
            std::fprintf(stderr, "init failed on %s @ 0x%02lx: %s\n", device.c_str(), address, toString(err));
            return 1;
        }
        std::printf("BME280 ready on %s @ 0x%02lx, one measurement takes %u us\n", device.c_str(), address,
                    static_cast<unsigned>(sensor.measurementTimeUs()));

        int result = 0;
        for (unsigned long i = 0; (count == 0 || i < count) && g_stop == 0; ++i) {
            if (i > 0) {
                waitMs(clock, interval);
            }
            bme280::Measurement m;
            const bme280::Error readErr = sensor.readForced(m);
            if (readErr != bme280::Error::None) {
                std::fprintf(stderr, "read failed: %s\n", toString(readErr));
                result = 2;
                continue;
            }
            std::printf("T = %6.2f C   p = %9.0f Pa   RH = %5.1f %%\n", static_cast<double>(m.temperatureC),
                        static_cast<double>(m.pressurePa), static_cast<double>(m.humidityPct));
            std::fflush(stdout);
        }
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
