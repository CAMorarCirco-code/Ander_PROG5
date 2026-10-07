#pragma once

// Publisher on top of libmosquitto (RAII). The constructor starts the MQTT
// network loop on mosquitto's own thread and *initiates* an asynchronous
// connection to the broker; it does not wait for, or guarantee, an
// established connection. The destructor disconnects and stops that thread.
// mosquitto.h stays in the .cpp: users of this header (main) do not depend on
// the MQTT library.
//
//   sudo apt install libmosquitto-dev mosquitto mosquitto-clients   (Pi / Debian)

#include "Publisher.hpp"

#include <atomic>
#include <string>

struct mosquitto;   // forward declaration

namespace station {

class MqttPublisher final : public Publisher {
public:
    /// Starts the network loop, then initiates an asynchronous connect and
    /// returns at once. The connection is established later on the network
    /// thread (see isConnected()); if the broker is down, that thread keeps
    /// retrying (1 s .. 10 s back-off) and publish() returns false meanwhile.
    MqttPublisher(const std::string& host, int port, const std::string& clientId = "bme280-station");
    ~MqttPublisher() override;

    MqttPublisher(const MqttPublisher&)            = delete;
    MqttPublisher& operator=(const MqttPublisher&) = delete;

    bool isConnected() const { return connected_; }

    /// QoS 0, not retained. Returns false when not connected or on error;
    /// never blocks on the network (it only queues the message).
    bool publish(const std::string& topic, const std::string& payload) override;

private:
    static void onConnect(mosquitto* client, void* self, int rc);
    static void onDisconnect(mosquitto* client, void* self, int rc);

    mosquitto*        client_ = nullptr;
    std::atomic<bool> connected_{false};   // written by mosquitto's thread, read by the sampler's
};

} // namespace station
