// Composition root of the weather station (assignment 5, step 3): the one
// place where the concrete classes meet. Everything else talks to
// interfaces: Sampler -> EnvironmentSensor, callback -> Publisher.
//
//   station_main                    BME280 on /dev/i2c-1 @ 0x76, MQTT to localhost:1883
//   station_main --fake             FakeSensor instead of the BME280 (works on a laptop)
//   station_main --console          print instead of MQTT
//   station_main --fake --console   no hardware, no broker
//
// Verify:  mosquitto_sub -h localhost -t 'han/ese/#' -v
// Stop:    Ctrl+C (SIGINT) or SIGTERM.
//
// No #ifdef, no globals, no singletons: the choice of sensor and publisher
// is made at run time, and Ctrl+C is received with sigwait() instead of a
// signal handler writing a global flag.

#include <bme280/Bme280.hpp>
#include <station/ConsolePublisher.hpp>
#include <station/FakeSensor.hpp>
#include <station/Json.hpp>
#include <station/MqttPublisher.hpp>
#include <station/Sampler.hpp>

#include "LinuxClock.hpp"
#include "LinuxI2cBus.hpp"

#include <pthread.h>
#include <signal.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <system_error>

int main(int argc, char** argv)
{
    // Block SIGINT/SIGTERM before any thread exists (the sampler's and
    // mosquitto's inherit the mask), so only sigwait() below receives them.
    sigset_t stopSignals;
    sigemptyset(&stopSignals);
    sigaddset(&stopSignals, SIGINT);
    sigaddset(&stopSignals, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &stopSignals, nullptr);

    bool useFake    = false;
    bool useConsole = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--fake") == 0) {
            useFake = true;
        } else if (std::strcmp(argv[i], "--console") == 0) {
            useConsole = true;
        } else {
            std::fprintf(stderr, "usage: %s [--fake] [--console]\n", argv[0]);
            return 1;
        }
    }
    const std::string topic = "han/ese/ander/bme280/state";

    // --- wiring: the sampler only sees EnvironmentSensor&, the callback only Publisher&
    auto sampleAndPublish = [&](station::EnvironmentSensor& sensor, station::Publisher& publisher) -> int {
        // init() on the main thread, before the sampler thread exists; from
        // start() to stop() the sampler is the sensor's only user.
        if (sensor.init() != bme280::Error::None) {
            std::fprintf(stderr, "sensor init failed\n");
            return 2;
        }
        station::Sampler sampler(sensor, std::chrono::seconds(1));
        sampler.onMeasurement([&](const bme280::Measurement& m) {   // the lambda is the callback
            if (!publisher.publish(topic, station::toJson(m))) {   // runs on the sampler thread
                std::fprintf(stderr, "publish failed (broker down?)\n");
            }
        });
        sampler.start();

        int signal = 0;
        sigwait(&stopSignals, &signal);   // main sleeps here; it never touches the sensor
        sampler.stop();
        std::fprintf(stderr, "stopped (%s)\n", signal == SIGINT ? "SIGINT" : "SIGTERM");
        return 0;
    };

    // --- publisher ------------------------------------------------------------
    auto withPublisher = [&](station::EnvironmentSensor& sensor) -> int {
        if (useConsole) {
            station::ConsolePublisher publisher;
            return sampleAndPublish(sensor, publisher);
        }
        station::MqttPublisher publisher("localhost", 1883);
        return sampleAndPublish(sensor, publisher);
    };

    // --- sensor ---------------------------------------------------------------
    if (useFake) {
        station::FakeSensor sensor;
        return withPublisher(sensor);
    }
    try {
        bme280::LinuxI2cBus bus(static_cast<uint8_t>(bme280::I2cAddress::Low));   // /dev/i2c-1
        bme280::LinuxClock  sysClock;   // not `clock`: C already owns that name
        bme280::Bme280      sensor(bus, sysClock);
        return withPublisher(sensor);
    } catch (const std::system_error& e) {
        std::fprintf(stderr, "%s (no sensor? try --fake)\n", e.what());
        return 1;
    }
    // Destruction order on every path: ~Sampler (already stopped), then the
    // publisher (~MqttPublisher disconnects), then sensor, clock and bus
    // (~LinuxI2cBus closes the fd).
}
