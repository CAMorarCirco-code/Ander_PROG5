#pragma once

// Arduino entry point: `#include <Bme280Lib.h>` in a sketch.
//
// The Arduino IDE only puts a library's src/ folder on the include path and
// only compiles files below src/. This header and the two arduino_glue_*
// files make the mandatory layout (include/, platform/, third_party/) work
// there. They are Arduino build glue, not part of the core: CMake never
// uses them, and the core (include/, src/Bme280.cpp) does not include them.

#include "../include/bme280/Bme280.hpp"
#include "../platform/arduino/ArduinoI2cBus.hpp"
