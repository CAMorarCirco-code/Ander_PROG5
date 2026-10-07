#include "station/MqttPublisher.hpp"

#include <mosquitto.h>

namespace station {

MqttPublisher::MqttPublisher(const std::string& host, int port, const std::string& clientId)
{
    mosquitto_lib_init();
    client_ = mosquitto_new(clientId.c_str(), /*clean_session=*/true, this);
    if (client_ == nullptr) {
        return;   // publish() will return false
    }
    mosquitto_connect_callback_set(client_, &MqttPublisher::onConnect);
    mosquitto_disconnect_callback_set(client_, &MqttPublisher::onDisconnect);
    mosquitto_reconnect_delay_set(client_, 1, 10, /*exponential=*/true);

    // Start the network thread *before* connecting: then the connection
    // attempt belongs to that thread, and a broker that is down now is
    // retried until it comes up. (With libmosquitto 2.0.18, a failed
    // connect_async() before loop_start() is never retried.)
    // Neither call blocks on the broker, so the constructor returns at once.
    if (mosquitto_loop_start(client_) == MOSQ_ERR_SUCCESS) {
        mosquitto_connect_async(client_, host.c_str(), port, /*keepalive=*/60);
    }
}

MqttPublisher::~MqttPublisher()
{
    if (client_ != nullptr) {
        mosquitto_disconnect(client_);                  // also ends the retry loop if never connected
        mosquitto_loop_stop(client_, /*force=*/false);  // join mosquitto's thread
        mosquitto_destroy(client_);
    }
    mosquitto_lib_cleanup();
}

bool MqttPublisher::publish(const std::string& topic, const std::string& payload)
{
    if (client_ == nullptr || !connected_) {
        return false;
    }
    const int rc = mosquitto_publish(client_, nullptr, topic.c_str(), static_cast<int>(payload.size()),
                                     payload.data(), /*qos=*/0, /*retain=*/false);
    return rc == MOSQ_ERR_SUCCESS;
}

// --- C callbacks on mosquitto's thread: `self` is the `this` given to mosquitto_new
void MqttPublisher::onConnect(mosquitto*, void* self, int rc)
{
    static_cast<MqttPublisher*>(self)->connected_ = (rc == 0);
}

void MqttPublisher::onDisconnect(mosquitto*, void* self, int)
{
    static_cast<MqttPublisher*>(self)->connected_ = false;
}

} // namespace station
