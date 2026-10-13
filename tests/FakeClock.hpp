#pragma once

// Week 7: a Clock that only counts. Records every requested delay so a test
// can check *that* the sensor waited and *how long*, without sleeping.

#include <hal/Clock.hpp>

#include <algorithm>
#include <vector>

namespace test {

class FakeClock final : public hal::Clock {
public:
    void delayUs(uint32_t us) override { delays_.push_back(us); }

    const std::vector<uint32_t>& delays() const { return delays_; }
    uint32_t                     longest() const
    {
        return delays_.empty() ? 0U : *std::max_element(delays_.begin(), delays_.end());
    }
    void clear() { delays_.clear(); }

private:
    std::vector<uint32_t> delays_;
};

} // namespace test
