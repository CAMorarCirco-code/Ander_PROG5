# Composition root (week 5): BME280 on Linux or FakeSensor -> Sampler ->
# lambda -> ConsolePublisher or MqttPublisher.
add_executable(station_main station_main.cpp)
target_link_libraries(station_main PRIVATE bme280_station_mqtt bme280_linux)
bme280_set_warnings(station_main)
