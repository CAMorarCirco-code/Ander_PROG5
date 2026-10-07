# Linux userspace platform: /dev/i2c-N via i2c-dev, clock_nanosleep().
# Depends on the core's interfaces; the core does not know this exists.
add_library(bme280_linux STATIC LinuxI2cBus.cpp LinuxClock.cpp)
add_library(bme280::linux ALIAS bme280_linux)
target_include_directories(bme280_linux PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bme280_linux PUBLIC bme280_core)
bme280_set_warnings(bme280_linux)
