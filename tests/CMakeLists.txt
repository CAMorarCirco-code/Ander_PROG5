# Core tests: only bme280_core, a MockBus and a FakeClock.
add_executable(test_core test_core.cpp)
target_link_libraries(test_core PRIVATE bme280_core)
bme280_set_warnings(test_core)
add_test(NAME core COMMAND test_core)

# ArduinoI2cBus / ArduinoClock against a recording fake Wire (host only: the
# fake Arduino.h/Wire.h are on this target's include path and nowhere else).
add_executable(test_arduino_platform_fake
    test_arduino_platform_fake.cpp
    ${PROJECT_SOURCE_DIR}/platform/arduino/ArduinoI2cBus.cpp
    ${PROJECT_SOURCE_DIR}/platform/arduino/ArduinoClock.cpp)
target_include_directories(test_arduino_platform_fake PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fake_arduino
    ${PROJECT_SOURCE_DIR}/platform/arduino)
target_link_libraries(test_arduino_platform_fake PRIVATE bme280_core)
bme280_set_warnings(test_arduino_platform_fake)
add_test(NAME arduino_platform_fake COMMAND test_arduino_platform_fake)

if(BME280_BUILD_LINUX)
    # LinuxI2cBus against a fake ioctl() (message layout, end-to-end core).
    add_executable(test_linux_i2c_bus_fake test_linux_i2c_bus_fake.cpp)
    target_link_libraries(test_linux_i2c_bus_fake PRIVATE bme280_linux)
    bme280_set_warnings(test_linux_i2c_bus_fake)
    add_test(NAME linux_i2c_bus_fake COMMAND test_linux_i2c_bus_fake)

    # LinuxI2cBus / LinuxClock against the real kernel (no hardware needed).
    add_executable(test_linux_platform test_linux_platform.cpp)
    target_link_libraries(test_linux_platform PRIVATE bme280_linux)
    bme280_set_warnings(test_linux_platform)
    add_test(NAME linux_platform COMMAND test_linux_platform)
endif()

if(BME280_BUILD_STATION)
    # Sampler lifecycle/threading, FakeSensor, JSON - no hardware, no broker.
    add_executable(test_station test_station.cpp)
    target_link_libraries(test_station PRIVATE bme280_station)
    bme280_set_warnings(test_station)
    add_test(NAME station COMMAND test_station)
endif()

if(BME280_BUILD_STATION AND BME280_HAVE_MOSQUITTO)
    # MqttPublisher: broker down, and a real broker the test starts itself.
    add_executable(test_mqtt_publisher test_mqtt_publisher.cpp)
    target_include_directories(test_mqtt_publisher PRIVATE ${MOSQUITTO_INCLUDE_DIR})
    target_link_libraries(test_mqtt_publisher PRIVATE bme280_station_mqtt ${MOSQUITTO_LIBRARY})
    bme280_set_warnings(test_mqtt_publisher)
    find_program(MOSQUITTO_BROKER mosquitto PATHS /usr/sbin /usr/local/sbin /opt/homebrew/sbin)
    if(MOSQUITTO_BROKER AND NOT CMAKE_CROSSCOMPILING)
        add_test(NAME mqtt_publisher COMMAND test_mqtt_publisher ${MOSQUITTO_BROKER})
    else()
        add_test(NAME mqtt_publisher COMMAND test_mqtt_publisher)   # broker tests SKIPPED
    endif()
endif()

if(TARGET station_main AND NOT CMAKE_CROSSCOMPILING)
    # The composition root end to end: console, no hardware, broker down,
    # real broker + mosquitto_sub. Exit 77 = skipped (no broker / port busy).
    find_program(MOSQUITTO_SUB mosquitto_sub)
    add_test(NAME station_e2e
             COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/station_e2e.sh $<TARGET_FILE:station_main>
                     ${MOSQUITTO_BROKER} ${MOSQUITTO_SUB})
    set_tests_properties(station_e2e PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)
endif()

if(BME280_BUILD_STATION)
    # Week 7, assignment 7: required unit tests with doctest (fetched by CMake,
    # as in the week-2 starter). Offline: pass
    # -DFETCHCONTENT_SOURCE_DIR_DOCTEST=/path/to/doctest-2.4.11
    include(FetchContent)
    FetchContent_Declare(doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG        v2.4.11
        GIT_SHALLOW    TRUE)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)   # doctest 2.4.11 declares cmake_minimum_required(3.0)
    set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(doctest)

    add_executable(test_unit test_unit.cpp)
    target_link_libraries(test_unit PRIVATE bme280_station doctest::doctest)
    bme280_set_warnings(test_unit)
    add_test(NAME unit COMMAND test_unit)
endif()
