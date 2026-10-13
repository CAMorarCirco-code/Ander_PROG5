#include "station/Sampler.hpp"

#include <utility>

namespace station {

Sampler::Sampler(std::vector<hal::EnvironmentSensor*> sensors, std::chrono::milliseconds period)
    : sensors_(std::move(sensors)), period_(period)
{
}

Sampler::Sampler(hal::EnvironmentSensor& sensor, std::chrono::milliseconds period)
    : Sampler(std::vector<hal::EnvironmentSensor*>{&sensor}, period)
{
}

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

        // One cycle: every sensor once, in order, through the interface only.
        for (std::size_t i = 0; i < sensors_.size(); ++i) {
            if (i > 0 && !isRunning()) {
                return;             // stop() during a cycle: do not start the next read
            }
            hal::Measurement m;
            if (sensors_[i] != nullptr && sensors_[i]->readForced(m) == hal::Error::None && callback) {
                callback(i, m);     // sampler thread, not main
            }
        }

        // Next deadline counted from the start, so slow reads do not shift
        // every later cycle. If we are already late, do not try to catch up
        // with a burst of cycles.
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
