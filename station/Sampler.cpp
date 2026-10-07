#include "station/Sampler.hpp"

#include <utility>

namespace station {

Sampler::Sampler(EnvironmentSensor& sensor, std::chrono::milliseconds period) : sensor_(sensor), period_(period) {}

Sampler::~Sampler()
{
    stop();
}

void Sampler::onMeasurement(Callback callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(callback);
}

void Sampler::start()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_ || thread_.joinable()) {
        return;
    }
    running_ = true;
    thread_  = std::thread(&Sampler::run, this);
}

void Sampler::stop()
{
    {
        // Set the flag under the mutex: otherwise it could change between
        // run() checking the predicate and starting to wait, and the
        // notify below would be lost (stop() would then wait a full period).
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
    }
    wakeUp_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

bool Sampler::isRunning() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
}

void Sampler::run()
{
    auto next = std::chrono::steady_clock::now();
    while (true) {
        Callback callback;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) {
                return;
            }
            callback = callback_;   // copy: onMeasurement() may replace it meanwhile
        }

        bme280::Measurement m;
        if (sensor_.readForced(m) == bme280::Error::None && callback) {
            callback(m);            // sampler thread, not main
        }

        // Next deadline counted from the start, so a slow read does not
        // shift every later sample. If we are already late, do not try to
        // catch up with a burst of reads.
        next += period_;
        const auto now = std::chrono::steady_clock::now();
        if (next < now) {
            next = now;
        }
        std::unique_lock<std::mutex> lock(mutex_);
        wakeUp_.wait_until(lock, next, [this] { return !running_; });
    }
}

} // namespace station
