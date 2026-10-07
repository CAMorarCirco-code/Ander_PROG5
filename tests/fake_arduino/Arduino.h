#pragma once

// Host stand-in for the Arduino core, only what platform/arduino/ uses.
// Lives under tests/ and is only on the include path of the Arduino
// platform test; the library and the real sketches never see it.

#include <cstdint>

void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
