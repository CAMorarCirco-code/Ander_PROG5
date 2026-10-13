# Linux userspace platform: /dev/i2c-N via i2c-dev, clock_nanosleep().
# Depends only on hal (the interfaces); no sensor package knows this exists.
add_library(bme280_linux STATIC LinuxI2cBus.cpp LinuxClock.cpp)
add_library(bme280::linux ALIAS bme280_linux)
target_include_directories(bme280_linux PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bme280_linux PUBLIC hal)   # implements hal::Bus/Clock; no BME280 dependency
bme280_set_warnings(bme280_linux)
