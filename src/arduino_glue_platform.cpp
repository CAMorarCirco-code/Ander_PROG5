// Arduino build glue: compiles the Arduino platform layer, which lives in
// platform/arduino/ and would otherwise be skipped by the Arduino IDE.
// Empty on every other platform. platform/linux/ is never compiled here.
#if defined(ARDUINO)
#include "../platform/arduino/ArduinoI2cBus.cpp"
#include "../platform/arduino/ArduinoClock.cpp"
#endif
