# Station layer (week 5): Sampler thread + callback, Publisher interface,
# ConsolePublisher, FakeSensor, JSON. Portable C++17 + std::thread; no
# platform code, no MQTT library (that is bme280_station_mqtt).
add_library(bme280_station STATIC Sampler.cpp)
target_link_libraries(bme280_station PUBLIC bme280_core Threads::Threads)
bme280_set_warnings(bme280_station)

# MqttPublisher: the only code that touches libmosquitto. Built only if the
# library is found, so the rest of the station (and the cross builds) do not
# depend on it.
find_path(MOSQUITTO_INCLUDE_DIR mosquitto.h)
find_library(MOSQUITTO_LIBRARY mosquitto)
if(MOSQUITTO_INCLUDE_DIR AND MOSQUITTO_LIBRARY)
    add_library(bme280_station_mqtt STATIC MqttPublisher.cpp)
    target_include_directories(bme280_station_mqtt PRIVATE ${MOSQUITTO_INCLUDE_DIR})
    target_link_libraries(bme280_station_mqtt PUBLIC bme280_station PRIVATE ${MOSQUITTO_LIBRARY})
    bme280_set_warnings(bme280_station_mqtt)
    set(BME280_HAVE_MOSQUITTO ON PARENT_SCOPE)
    set(BME280_HAVE_MOSQUITTO ON)
else()
    message(STATUS "libmosquitto not found: MqttPublisher and bme280_station are not built "
                   "(apt install libmosquitto-dev)")
    set(BME280_HAVE_MOSQUITTO OFF PARENT_SCOPE)
endif()
