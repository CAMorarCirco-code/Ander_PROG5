#pragma once

// Register-level model of a BME280, shared by the core tests (behind a
// MockBus) and the Linux bus tests (behind a fake ioctl). It is loaded with
// the compensation example from the Bosch datasheet (BST-BME280-DS002,
// section 8.1), so the expected temperature/pressure are known in advance.

#include <array>
#include <cstddef>
#include <cstdint>

namespace test {

class FakeBme280 {
public:
    // Datasheet example: adc_T = 519888 -> 25.08 degC,
    //                    adc_P = 415148 -> ~100653 Pa.
    static constexpr uint32_t kAdcT = 519888;
    static constexpr uint32_t kAdcP = 415148;
    static constexpr uint32_t kAdcH = 30000;

    FakeBme280() { reset(); }

    std::array<uint8_t, 256> regs{};
    uint8_t chipId = 0x60;

    /// Burst read from consecutive registers, like the real chip.
    void read(uint8_t reg, uint8_t* data, size_t len) const
    {
        for (size_t i = 0; i < len; ++i) {
            const auto r = static_cast<uint8_t>(reg + i);
            data[i] = (r == 0xD0) ? chipId : regs[r];
        }
    }

    /// I2C write payload as the Bosch driver produces it: the first value
    /// goes to `reg`, further bytes are (register, value) pairs.
    void write(uint8_t reg, const uint8_t* data, size_t len)
    {
        if (len == 0) {
            return;
        }
        store(reg, data[0]);
        for (size_t i = 1; i + 1 < len; i += 2) {
            store(data[i], data[i + 1]);
        }
    }

    unsigned softResets = 0;
    unsigned forcedTriggers = 0;

private:
    void store(uint8_t reg, uint8_t value)
    {
        if (reg == 0xE0) {
            if (value == 0xB6) {
                ++softResets;
                reset();
            }
            return;
        }
        if (reg == 0xF4 && (value & 0x03) != 0 && (value & 0x03) != 0x03) {
            // Forced mode: the measurement "completes" and the chip drops back
            // to sleep, keeping the oversampling bits.
            ++forcedTriggers;
            value = static_cast<uint8_t>(value & ~0x03);
        }
        regs[reg] = value;
    }

    void put16(uint8_t reg, uint16_t v)
    {
        regs[reg]                          = static_cast<uint8_t>(v & 0xFF);
        regs[static_cast<uint8_t>(reg + 1)] = static_cast<uint8_t>(v >> 8);
    }

    void put20(uint8_t reg, uint32_t adc)
    {
        regs[reg]                          = static_cast<uint8_t>(adc >> 12);
        regs[static_cast<uint8_t>(reg + 1)] = static_cast<uint8_t>((adc >> 4) & 0xFF);
        regs[static_cast<uint8_t>(reg + 2)] = static_cast<uint8_t>((adc & 0x0F) << 4);
    }

    void reset()
    {
        regs.fill(0);
        // Temperature / pressure calibration, 0x88..0x9F, little endian.
        put16(0x88, 27504);                            // dig_T1
        put16(0x8A, 26435);                            // dig_T2
        put16(0x8C, static_cast<uint16_t>(-1000));     // dig_T3
        put16(0x8E, 36477);                            // dig_P1
        put16(0x90, static_cast<uint16_t>(-10685));    // dig_P2
        put16(0x92, 3024);                             // dig_P3
        put16(0x94, 2855);                             // dig_P4
        put16(0x96, 140);                              // dig_P5
        put16(0x98, static_cast<uint16_t>(-7));        // dig_P6
        put16(0x9A, 15500);                            // dig_P7
        put16(0x9C, static_cast<uint16_t>(-14600));    // dig_P8
        put16(0x9E, 6000);                             // dig_P9
        // Humidity calibration (typical values, no datasheet example).
        regs[0xA1] = 75;                               // dig_H1
        put16(0xE1, 362);                              // dig_H2
        regs[0xE3] = 0;                                // dig_H3
        regs[0xE4] = 313 >> 4;                         // dig_H4 [11:4]
        regs[0xE5] = static_cast<uint8_t>((313 & 0x0F) | ((50 & 0x0F) << 4));
        regs[0xE6] = 50 >> 4;                          // dig_H5 [11:4]
        regs[0xE7] = 30;                               // dig_H6
        // Raw data 0xF7..0xFE: press[19:0], temp[19:0], hum[15:0].
        put20(0xF7, kAdcP);
        put20(0xFA, kAdcT);
        regs[0xFD] = static_cast<uint8_t>(kAdcH >> 8);
        regs[0xFE] = static_cast<uint8_t>(kAdcH & 0xFF);
        regs[0xF3] = 0;                                // status: NVM copy done
    }
};

} // namespace test
