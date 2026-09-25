/* Arduino build glue: compiles the vendored, unmodified Bosch driver, which
 * lives outside src/ and would otherwise be skipped by the Arduino IDE.
 * Same compensation mode as the wrapper (see Bme280.hpp). */
#if defined(ARDUINO)
#ifndef BME280_32BIT_ENABLE
#define BME280_32BIT_ENABLE
#endif
#include "../third_party/bme280/bme280.c"
#endif
