#pragma once

// Week 7: the MockBus of the week-2 starter (a 256-byte register map plus a
// log of what was written), adapted to this library:
//  * Bus has no delayUs() since week 4 - timing is FakeClock's job.
//  * The Bosch driver writes several registers in one call as
//    [value0, reg1, value1, reg2, value2, ...] after the first register
//    (bme280_set_regs). The starter stored data[i] at reg+i, which turns a
//    burst write into garbage; here the pairs are decoded.
// LSP: fully substitutable for Bus.

#include <hal/Bus.hpp>

#include <array>
#include <cstring>
#include <vector>

namespace test {

class MockBus final : public hal::Bus {
public:
    static constexpr uint8_t kRegChipId    = 0xD0;
    static constexpr uint8_t kChipIdBme280 = 0x60;

    MockBus() { regs_[kRegChipId] = kChipIdBme280; }

    bool read(uint8_t reg, uint8_t* data, size_t len) override
    {
        ++reads_;
        if (failNextRead_) {
            failNextRead_ = false;
            return false;
        }
        if (data == nullptr || len == 0 || reg + len > regs_.size()) {
            return false;
        }
        std::memcpy(data, &regs_[reg], len);
        return true;
    }

    bool write(uint8_t reg, const uint8_t* data, size_t len) override
    {
        if (failNextWrite_) {
            failNextWrite_ = false;
            return false;
        }
        if (data == nullptr || len == 0) {
            return false;
        }
        store(reg, data[0]);
        for (size_t i = 1; i + 1 < len; i += 2) {
            store(data[i], data[i + 1]);
        }
        return true;
    }

    // --- test helpers ---------------------------------------------------------
    struct Write {
        uint8_t reg;
        uint8_t value;
    };

    uint8_t& reg(uint8_t addr) { return regs_[addr]; }
    void     put16(uint8_t addr, uint16_t v)   // little endian, like the calibration block
    {
        regs_[addr]                            = static_cast<uint8_t>(v & 0xFF);
        regs_[static_cast<uint8_t>(addr + 1)] = static_cast<uint8_t>(v >> 8);
    }
    const std::vector<Write>& writes() const { return writes_; }
    unsigned reads() const { return reads_; }
    void     failNextRead() { failNextRead_ = true; }
    void     failNextWrite() { failNextWrite_ = true; }

private:
    void store(uint8_t reg, uint8_t value)
    {
        regs_[reg] = value;
        writes_.push_back({reg, value});
    }

    std::array<uint8_t, 256> regs_{};
    std::vector<Write>       writes_;
    unsigned                 reads_         = 0;
    bool                     failNextRead_  = false;
    bool                     failNextWrite_ = false;
};

} // namespace test
