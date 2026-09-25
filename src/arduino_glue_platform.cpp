// Arduino build glue: compiles the Arduino platform layer, which lives in
// platform/arduino/ and would otherwise be skipped by the Arduino IDE.
// Empty on every other platform.
#if defined(ARDUINO)
#include "../platform/arduino/ArduinoI2cBus.cpp"
#endif
