cmake_minimum_required(VERSION 3.16)
project(Bme280Lib VERSION 0.4.0 LANGUAGES C CXX)

# Target structure (platform choice is made here, never inside the core):
#
#   bme280_bosch   Bosch SensorAPI (C, vendored)        <- any platform
#   bme280_core    Bme280 + Bus/Clock interfaces       <- any platform
#   bme280_linux   LinuxI2cBus + LinuxClock (i2c-dev)  <- Linux only
#   bme280_rpi_smoke                                   <- Linux only
#   bme280_station Sampler (thread + callback),        <- any platform with
#                  Publisher, FakeSensor, JSON            std::thread
#
# The Arduino platform is built by the Arduino IDE / arduino-cli from
# library.properties and src/, not by CMake.

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_bme280_is_linux ON)
else()
    set(_bme280_is_linux OFF)
endif()
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(_bme280_top_level ON)
else()
    set(_bme280_top_level OFF)
endif()

option(BME280_BUILD_LINUX       "Build the Linux i2c-dev platform (LinuxI2cBus, LinuxClock)" ${_bme280_is_linux})
option(BME280_BUILD_STATION     "Build the week-5 station layer (Sampler, publishers)"         ON)
option(BME280_BUILD_EXAMPLES    "Build example applications"                                ${_bme280_top_level})
option(BME280_BUILD_TESTS       "Build host tests"                                          ${_bme280_top_level})
option(BME280_WARNINGS_AS_ERRORS "Treat compiler warnings as errors"                        OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

function(bme280_set_warnings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(BME280_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# --- Bosch BME280 SensorAPI (unmodified) -------------------------------------
add_library(bme280_bosch STATIC third_party/bme280/bme280.c)
# Bme280.cpp converts the 32-bit integer compensation output (see Bme280.hpp).
target_compile_definitions(bme280_bosch PUBLIC BME280_32BIT_ENABLE)
target_include_directories(bme280_bosch PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/bme280)
bme280_set_warnings(bme280_bosch)

# --- Core: platform independent ----------------------------------------------
add_library(bme280_core STATIC src/Bme280.cpp)
add_library(bme280::core ALIAS bme280_core)
target_include_directories(bme280_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(bme280_core PUBLIC bme280_bosch)
bme280_set_warnings(bme280_core)

# --- Station (week 5) -------------------------------------------------------
if(BME280_BUILD_STATION)
    find_package(Threads REQUIRED)
    add_subdirectory(station)
endif()

# --- Platforms ---------------------------------------------------------------
if(BME280_BUILD_LINUX)
    add_subdirectory(platform/linux)
endif()

# --- Examples / tests --------------------------------------------------------
if(BME280_BUILD_EXAMPLES AND BME280_BUILD_LINUX)
    add_subdirectory(examples/rpi)
endif()

if(BME280_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
