// MqttPublisher against no broker and against a real mosquitto broker that
// this test starts and stops itself on a free localhost port.
//
//   test_mqtt_publisher [path/to/mosquitto]
//
// Without the broker path only the broker-down tests run; the others are
// reported as SKIPPED, not as passed.

#include "station/FakeSensor.hpp"
#include "station/Json.hpp"
#include "station/MqttPublisher.hpp"
#include "station/Sampler.hpp"
#include "TestMain.hpp"

#include <mosquitto.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <csignal>
#include <memory>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

extern char** environ;

namespace {

using namespace std::chrono_literals;
using SteadyClock = std::chrono::steady_clock;

const std::string kTopic = "han/ese/ander/bme280/state";
std::string       g_brokerPath;   // from argv; empty: broker tests skipped

double secondsSince(SteadyClock::time_point t0)
{
    return std::chrono::duration<double>(SteadyClock::now() - t0).count();
}

template <typename Pred>
bool waitUntil(Pred pred, std::chrono::milliseconds timeout)
{
    const auto deadline = SteadyClock::now() + timeout;
    while (!pred()) {
        if (SteadyClock::now() > deadline) {
            return false;
        }
        std::this_thread::sleep_for(10ms);
    }
    return true;
}

int freePort()
{
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = 0;
    socklen_t len        = sizeof addr;
    ::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr);
    ::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len);
    ::close(fd);
    return ntohs(addr.sin_port);
}

bool portAcceptsConnections(int port)
{
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = htons(static_cast<uint16_t>(port));
    const bool ok = ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) == 0;
    ::close(fd);
    return ok;
}

/// A real mosquitto process on `port`, killed in the destructor.
class Broker {
public:
    explicit Broker(int port) : port_(port)
    {
        const std::string portText = std::to_string(port);
        std::vector<char*> argv{const_cast<char*>(g_brokerPath.c_str()), const_cast<char*>("-p"),
                                const_cast<char*>(portText.c_str()), nullptr};
        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_addopen(&actions, 1, "/dev/null", O_WRONLY, 0);
        posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0);
        if (posix_spawn(&pid_, g_brokerPath.c_str(), &actions, nullptr, argv.data(), environ) != 0) {
            pid_ = -1;
        }
        posix_spawn_file_actions_destroy(&actions);
        up_ = pid_ > 0 && waitUntil([&] { return portAcceptsConnections(port_); }, 5000ms);
    }
    ~Broker()
    {
        if (pid_ > 0) {
            ::kill(pid_, SIGTERM);
            ::waitpid(pid_, nullptr, 0);
        }
    }
    Broker(const Broker&)            = delete;
    Broker& operator=(const Broker&) = delete;
    bool up() const { return up_; }

private:
    int   port_;
    pid_t pid_ = -1;
    bool  up_  = false;
};

/// Test-side subscriber on 'han/ese/#' (what mosquitto_sub -t 'han/ese/#' does).
class Subscriber {
public:
    explicit Subscriber(int port)
    {
        client_ = mosquitto_new("test-subscriber", true, this);
        mosquitto_connect_callback_set(client_, [](mosquitto* c, void*, int rc) {
            if (rc == 0) {
                mosquitto_subscribe(c, nullptr, "han/ese/#", 0);
            }
        });
        mosquitto_subscribe_callback_set(client_, [](mosquitto*, void* self, int, int, const int*) {
            static_cast<Subscriber*>(self)->subscribed_ = true;
        });
        mosquitto_message_callback_set(client_, [](mosquitto*, void* self, const mosquitto_message* msg) {
            auto* s = static_cast<Subscriber*>(self);
            std::lock_guard<std::mutex> lock(s->mutex_);
            s->messages_.emplace_back(msg->topic,
                                      std::string(static_cast<const char*>(msg->payload),
                                                  static_cast<size_t>(msg->payloadlen)));
        });
        mosquitto_loop_start(client_);
        mosquitto_connect_async(client_, "localhost", port, 60);
    }
    ~Subscriber()
    {
        mosquitto_disconnect(client_);
        mosquitto_loop_stop(client_, false);
        mosquitto_destroy(client_);
    }
    Subscriber(const Subscriber&)            = delete;
    Subscriber& operator=(const Subscriber&) = delete;

    bool waitSubscribed() { return waitUntil([&] { return subscribed_.load(); }, 5000ms); }
    std::vector<std::pair<std::string, std::string>> messages()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return messages_;
    }
    bool waitMessages(size_t n, std::chrono::milliseconds timeout = 5000ms)
    {
        return waitUntil([&] { return messages().size() >= n; }, timeout);
    }

private:
    mosquitto*                                       client_ = nullptr;
    std::atomic<bool>                                subscribed_{false};
    std::mutex                                       mutex_;
    std::vector<std::pair<std::string, std::string>> messages_;
};

// --- broker down (always runs) -----------------------------------------------

void brokerDownPublishReturnsFalseAndShutdownIsQuick()
{
    const int port = freePort();   // nobody listens here
    const auto t0  = SteadyClock::now();
    {
        station::MqttPublisher publisher("localhost", port, "test-down");
        CHECK(secondsSince(t0) < 1.0);          // constructor does not wait for a broker
        CHECK(!publisher.isConnected());
        CHECK(!publisher.publish(kTopic, "{}"));
        std::this_thread::sleep_for(200ms);
        CHECK(!publisher.publish(kTopic, "{}"));
    }
    CHECK(secondsSince(t0) < 2.0);              // destructor does not hang on retries
}

void samplerKeepsSamplingWhileBrokerDown()
{
    station::FakeSensor sensor;
    CHECK(sensor.init() == bme280::Error::None);
    station::MqttPublisher publisher("localhost", freePort(), "test-down-sampler");
    std::atomic<int> attempts{0};
    std::atomic<int> failures{0};
    station::Sampler sampler(sensor, std::chrono::milliseconds(5));
    sampler.onMeasurement([&](const bme280::Measurement& m) {
        ++attempts;
        if (!publisher.publish(kTopic, station::toJson(m))) {
            ++failures;
        }
    });
    sampler.start();
    CHECK(waitUntil([&] { return attempts >= 10; }, 5000ms));
    sampler.stop();
    CHECK(failures == attempts);
}

// --- real broker ---------------------------------------------------------------

void publishesExactTopicAndPayload()
{
    const int port = freePort();
    Broker broker(port);
    CHECK(broker.up());
    Subscriber sub(port);
    CHECK(sub.waitSubscribed());
    station::MqttPublisher publisher("localhost", port, "test-pub");
    CHECK(waitUntil([&] { return publisher.isConnected(); }, 5000ms));
    CHECK(publisher.publish(kTopic, R"({"t":21.37,"p":101325,"h":45.2})"));
    CHECK(sub.waitMessages(1));
    const auto msgs = sub.messages();
    CHECK(msgs.size() == 1);
    if (!msgs.empty()) {
        CHECK(msgs[0].first == kTopic);
        CHECK(msgs[0].second == R"({"t":21.37,"p":101325,"h":45.2})");
    }
}

void fullChainFakeSensorSamplerCallbackMqtt()
{
    // FakeSensor -> Sampler thread -> lambda -> toJson -> MqttPublisher -> broker -> subscriber
    const int port = freePort();
    Broker broker(port);
    CHECK(broker.up());
    Subscriber sub(port);
    CHECK(sub.waitSubscribed());
    station::MqttPublisher publisher("localhost", port, "test-chain");
    CHECK(waitUntil([&] { return publisher.isConnected(); }, 5000ms));

    station::FakeSensor sensor;
    CHECK(sensor.init() == bme280::Error::None);
    station::Sampler sampler(sensor, std::chrono::milliseconds(20));
    sampler.onMeasurement([&](const bme280::Measurement& m) { publisher.publish(kTopic, station::toJson(m)); });
    sampler.start();
    CHECK(sub.waitMessages(5));
    sampler.stop();

    // QoS 0 over one connection keeps order: the ramp arrives in sequence.
    station::FakeSensor reference;
    (void)reference.init();
    for (const auto& msg : sub.messages()) {
        bme280::Measurement expected;
        (void)reference.readForced(expected);
        CHECK(msg.first == kTopic);
        CHECK(msg.second == station::toJson(expected));
    }
}

void brokerDownAtStartThenComesUp()
{
    const int port = freePort();
    station::MqttPublisher publisher("localhost", port, "test-late-broker");
    CHECK(!publisher.publish(kTopic, "{}"));
    std::this_thread::sleep_for(500ms);
    Broker broker(port);
    CHECK(broker.up());
    const auto t0 = SteadyClock::now();
    CHECK(waitUntil([&] { return publisher.isConnected(); }, 20000ms));
    std::printf("  connected %.1f s after the broker came up\n", secondsSince(t0));
    CHECK(publisher.publish(kTopic, "{}"));
}

void brokerRestartWhileSampling()
{
    const int port = freePort();
    station::FakeSensor sensor;
    (void)sensor.init();
    std::atomic<int> ok{0};
    std::atomic<int> failed{0};
    auto broker = std::make_unique<Broker>(port);
    CHECK(broker->up());
    station::MqttPublisher publisher("localhost", port, "test-restart");
    CHECK(waitUntil([&] { return publisher.isConnected(); }, 5000ms));

    station::Sampler sampler(sensor, std::chrono::milliseconds(20));
    sampler.onMeasurement([&](const bme280::Measurement& m) {
        (publisher.publish(kTopic, station::toJson(m)) ? ok : failed)++;
    });
    sampler.start();
    CHECK(waitUntil([&] { return ok >= 3; }, 5000ms));

    broker.reset();                                                     // broker dies
    CHECK(waitUntil([&] { return !publisher.isConnected(); }, 5000ms));
    const int failedBefore = failed;
    CHECK(waitUntil([&] { return failed >= failedBefore + 5; }, 5000ms));   // still sampling, publish() false

    broker = std::make_unique<Broker>(port);                            // broker back
    CHECK(broker->up());
    const int okBefore = ok;
    CHECK(waitUntil([&] { return ok >= okBefore + 3; }, 25000ms));      // reconnected, publishing again
    sampler.stop();
}

void skipped(const char* name)
{
    std::printf("[ SKIP ] %s (no mosquitto broker binary given)\n", name);
}

} // namespace

int main(int argc, char** argv)
{
    std::signal(SIGPIPE, SIG_IGN);
    if (argc > 1) {
        g_brokerPath = argv[1];
    }
    RUN(brokerDownPublishReturnsFalseAndShutdownIsQuick);
    RUN(samplerKeepsSamplingWhileBrokerDown);
    if (g_brokerPath.empty()) {
        skipped("publishesExactTopicAndPayload");
        skipped("fullChainFakeSensorSamplerCallbackMqtt");
        skipped("brokerDownAtStartThenComesUp");
        skipped("brokerRestartWhileSampling");
    } else {
        RUN(publishesExactTopicAndPayload);
        RUN(fullChainFakeSensorSamplerCallbackMqtt);
        RUN(brokerDownAtStartThenComesUp);
        RUN(brokerRestartWhileSampling);
    }
    return TEST_RESULT();
}
