#pragma once

// Week 7: a Publisher that records instead of sending. Used where
// MqttPublisher cannot be unit-tested (it needs a broker): the Sampler is
// tested with a FakeSensor and this recorder. Thread-safe, because publish()
// is called from the sampler thread while the test thread waits.

#include <station/Publisher.hpp>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace test {

class RecordingPublisher final : public station::Publisher {
public:
    bool publish(const std::string& topic, const std::string& payload) override
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            messages_.emplace_back(topic, payload);
        }
        changed_.notify_all();
        return true;
    }

    bool waitFor(size_t n, std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return changed_.wait_for(lock, timeout, [&] { return messages_.size() >= n; });
    }

    std::vector<std::pair<std::string, std::string>> messages()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return messages_;
    }

private:
    std::mutex                                       mutex_;
    std::condition_variable                          changed_;
    std::vector<std::pair<std::string, std::string>> messages_;
};

} // namespace test
