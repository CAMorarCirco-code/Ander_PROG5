# Station layer (week 5): Sampler thread + callback, Publisher interface,
# ConsolePublisher, FakeSensor, JSON. Portable C++17 + std::thread; no
# platform code, no MQTT library (that is bme280_station_mqtt).
add_library(bme280_station STATIC Sampler.cpp)
target_link_libraries(bme280_station PUBLIC bme280_core Threads::Threads)
bme280_set_warnings(bme280_station)
