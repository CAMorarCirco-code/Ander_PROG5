#pragma once

// Measurement -> MQTT payload, the format of assignment 5:
//   {"t":21.37,"p":101325,"h":45.2}
// t in degC with 2 decimals, p in Pa without decimals, h in %RH with 1.
// snprintf uses the "C" locale (the program never calls setlocale), so the
// decimal separator is always '.'.

#include "../bme280/Types.hpp"

#include <cstdio>
#include <string>

namespace station {

inline std::string toJson(const bme280::Measurement& m)
{
    char buf[160];   // large enough even for FLT_MAX in every field
    std::snprintf(buf, sizeof buf, "{\"t\":%.2f,\"p\":%.0f,\"h\":%.1f}", static_cast<double>(m.temperatureC),
                  static_cast<double>(m.pressurePa), static_cast<double>(m.humidityPct));
    return buf;
}

} // namespace station
