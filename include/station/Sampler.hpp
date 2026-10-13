#pragma once

// Periodically reads a collection of EnvironmentSensors on its own thread and
// hands every measurement to a callback, together with the index of the sensor
// it came from. Owns the thread (composition): stop() joins it, the destructor
// calls stop(), the thread never outlives the Sampler.
//
// Week 6 (polymorphism): the sensors are bme280::EnvironmentSensor* - a real
// Bme280, a FakeSensor, or any other implementation. The sampler only calls
// the interface; no dynamic_cast, no "if (sensor is Bme280)".
//
// The sensors are borrowed, not owned: they must outlive the Sampler. While
// the sampler runs it is their only user (they are not thread-safe and do not
// need to be): init() them before start().

#include "../bme280/EnvironmentSensor.hpp"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace station {

class Sampler {
public:
    /// sensorIndex is the position of the sensor in the vector given to the
    /// constructor, so the owner can map it to a topic.
    using Callback = std::function<void(std::size_t sensorIndex, const bme280::Measurement&)>;

    /// One read of every sensor, in vector order, per `period`.
    Sampler(std::vector<bme280::EnvironmentSensor*> sensors, std::chrono::milliseconds period);

    /// Convenience for a single sensor (sensorIndex is always 0).
    Sampler(bme280::EnvironmentSensor& sensor, std::chrono::milliseconds period);

    ~Sampler();

    Sampler(const Sampler&)            = delete;
    Sampler& operator=(const Sampler&) = delete;

    /// The callback runs on the sampler thread, once per successful read.
    /// Normally set before start(); replacing it while running is safe.
    void onMeasurement(Callback callback);

    /// Start the sampling thread; no-op if already running. The first cycle
    /// happens immediately, then one every `period` (measured from start, so
    /// the read time does not add up).
    void start();

    /// Stop and join. Wakes a sleeping thread at once, so it returns after at
    /// most the read that is in progress, never after a full period; the
    /// remaining sensors of an interrupted cycle are not read. No callback runs
    /// after stop() has returned. Call it from the owner, not from the callback.
    void stop();

    bool isRunning() const;

    std::size_t sensorCount() const { return sensors_.size(); }

private:
    void run();
    bool keepRunning();

    const std::vector<bme280::EnvironmentSensor*> sensors_;
    const std::chrono::milliseconds               period_;

    mutable std::mutex      mutex_;     // guards running_ and callback_
    std::condition_variable wakeUp_;
    bool                    running_ = false;
    Callback                callback_;
    std::thread             thread_;    // last member: started after the rest exists
};

} // namespace station
