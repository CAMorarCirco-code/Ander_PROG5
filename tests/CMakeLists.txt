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
