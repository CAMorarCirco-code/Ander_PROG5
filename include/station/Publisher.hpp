#pragma once

// Where measurements go. main() wires the Sampler's callback to this
// abstraction; MQTT is a detail behind it (DIP).

#include <string>

namespace station {

class Publisher {
public:
    virtual ~Publisher() = default;

    /// Send `payload` to `topic`. Returns false if it could not be sent now
    /// (broker down, not connected). Must not block for long: it is called
    /// from the sampler thread.
    virtual bool publish(const std::string& topic, const std::string& payload) = 0;

protected:
    // Interfaces are used through references/pointers. Protected copy
    // operations stop slicing ("*a = *b" through the base) while still
    // letting implementations be copyable if they want to (same as Bus/Clock).
    Publisher()                            = default;
    Publisher(const Publisher&)            = default;
    Publisher& operator=(const Publisher&) = default;
};

} // namespace station
