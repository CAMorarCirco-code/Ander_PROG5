#pragma once

// Host stand-in for TwoWire: records every call so the test can check the
// exact transaction ArduinoI2cBus produces. Same member signatures as the
// ArduinoCore-API HardwareI2C/TwoWire used on SAMD.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class TwoWire {
public:
    // --- script for the next transactions ---------------------------------
    uint8_t              endResult = 0;     ///< endTransmission() return value
    int                  shortBy = 0;       ///< requestFrom() returns qty - shortBy
    std::vector<uint8_t> rxData;            ///< bytes the "device" sends back

    // --- record of what happened ------------------------------------------
    std::vector<std::string> calls;         ///< e.g. "begin 76", "end stop=0"
    std::vector<uint8_t>     tx;            ///< bytes of the current transmission
    std::vector<std::vector<uint8_t>> sent; ///< completed transmissions

    void beginTransmission(uint8_t address)
    {
        calls.push_back("begin " + hex(address));
        tx.clear();
    }
    size_t write(uint8_t b)
    {
        tx.push_back(b);
        return 1;
    }
    size_t write(const uint8_t* data, size_t len)
    {
        for (size_t i = 0; i < len; ++i) {
            tx.push_back(data[i]);
        }
        return len;
    }
    uint8_t endTransmission(bool stopBit = true)
    {
        calls.push_back(std::string("end stop=") + (stopBit ? "1" : "0"));
        sent.push_back(tx);
        return endResult;
    }
    uint8_t requestFrom(uint8_t address, uint8_t quantity)
    {
        calls.push_back("request " + hex(address) + " " + std::to_string(quantity));
        const int got = static_cast<int>(quantity) - shortBy;
        rxPos = 0;
        rxAvail = got < 0 ? 0 : static_cast<size_t>(got);
        return static_cast<uint8_t>(rxAvail);
    }
    int read()
    {
        if (rxPos >= rxAvail || rxPos >= rxData.size()) {
            return -1;
        }
        return rxData[rxPos++];
    }

private:
    static std::string hex(uint8_t v)
    {
        const char* digits = "0123456789abcdef";
        return std::string{digits[v >> 4], digits[v & 0x0F]};
    }
    size_t rxPos = 0;
    size_t rxAvail = 0;
};
