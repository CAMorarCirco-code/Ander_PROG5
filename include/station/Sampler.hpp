#pragma once

// Periodically reads an EnvironmentSensor on its own thread and hands every
// measurement to a callback. Owns the thread (composition): stop() joins it,
// the destructor calls stop(), the thread never outlives the Sampler.
//
// Single owner of the sensor: while the sampler runs, nobody else may call
// the sensor (it is not thread-safe and does not need to be).

#include "EnvironmentSensor.hpp"

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace station {

class Sampler {
public:
    using Callback = std::function<void(const bme280::Measurement&)>;

    Sampler(EnvironmentSensor& sensor, std::chrono::milliseconds period);
    ~Sampler();

    Sampler(const Sampler&)            = delete;
    Sampler& operator=(const Sampler&) = delete;

    /// The callback runs on the sampler thread, once per successful read.
    /// Normally set before start(); replacing it while running is safe.
    void onMeasurement(Callback callback);

    /// Start the sampling thread; no-op if already running. The first read
    /// happens immediately, then one every `period` (measured from start,
    /// so the read time does not add up).
    void start();

    /// Stop and join. Wakes a sleeping thread at once, so it returns after
    /// at most the read that is in progress, never after a full period.
    /// No callback runs after stop() has returned. Call it from the owner,
    /// not from inside the callback.
    void stop();

    bool isRunning() const;

private:
    void run();

    EnvironmentSensor&        sensor_;
    std::chrono::milliseconds period_;

    mutable std::mutex      mutex_;     // guards running_ and callback_
    std::condition_variable wakeUp_;
    bool                    running_ = false;
    Callback                callback_;
    std::thread             thread_;    // last member: started after the rest exists
};

} // namespace station
