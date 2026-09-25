# Ander_PROG5 - BME280 sensor library (Prog 5)

C++ library for the Bosch BME280 (temperature, pressure, humidity). The Bosch BME280 SensorAPI
is wrapped in a class that talks to an abstract `Bus`, so the sensor code does not depend on
any particular board. This is the Week 3 state: the core plus the Arduino platform.

## Structure

```
include/bme280/     Bus.hpp (interface), Types.hpp, Bme280.hpp   <- core: no Wire, no Linux
src/                Bme280.cpp (wraps the Bosch C driver)        <- core
                    Bme280Lib.h, arduino_glue_*.{c,cpp}           <- Arduino IDE build glue (see below)
third_party/bme280/ Bosch BME280 SensorAPI v3.5.1, vendored unmodified (BSD-3-Clause)
platform/arduino/   ArduinoI2cBus.hpp/.cpp (uses Wire)
examples/arduino/   ReadEverySecond (init() with defaults), CustomConfig (overrides one field)
library.properties
```

### Arduino IDE build glue

The Arduino IDE only compiles files under a library's `src/` folder and only puts `src/` on the
include path. To keep the mandatory layout, `src/` has three small glue files:

* `Bme280Lib.h` - what a sketch includes; it includes the core header and `ArduinoI2cBus.hpp`.
* `arduino_glue_bosch.c` - compiles `third_party/bme280/bme280.c`.
* `arduino_glue_platform.cpp` - compiles `platform/arduino/ArduinoI2cBus.cpp`.

The glue `.c`/`.cpp` files are wrapped in `#if defined(ARDUINO)`, so they are empty on any
other platform. The core itself (`include/bme280/*`, `src/Bme280.cpp`) includes none of them.

## Building for Arduino

Target: SAMD21 boards (`architectures=samd`, e.g. Arduino Nano 33 IoT, MKR, Zero). The AVR core
does not ship `<cstdint>`/`<cstddef>`, which the core headers use.

1. Copy or symlink this repository into your Arduino `libraries/` folder as `Bme280Lib`.
2. Open *File -> Examples -> Bme280Lib -> arduino -> ReadEverySecond*.
3. Wiring: VIN -> 3V3, GND -> GND, SDA -> SDA, SCL -> SCL. SDO open or to GND = `0x76`,
   SDO to 3V3 = `0x77` (then change `I2cAddress::Low` to `I2cAddress::High` in the sketch).
4. Upload and open the Serial Monitor at 115200 baud.

With `arduino-cli`:

```bash
arduino-cli compile -b arduino:samd:nano_33_iot --library . examples/arduino/ReadEverySecond
```

### Serial output

Not included yet. No Arduino board with a BME280 was available when this was written, so the
sketch has only been compiled, not run. See "Verification" below.

## Design

The core (`include/`, `src/Bme280.cpp`) only knows the abstract `Bus`, so it builds on any
platform with a C++17 compiler and can be tested on a laptop with a mock bus. `Wire` is an
Arduino detail: putting it in the sensor class would tie the sensor to one board family, and
next week the same sensor has to run on a Raspberry Pi through Linux `i2c-dev`. Whoever owns
the bus (the sketch) creates it and calls `Wire.begin()`; the sensor only receives a `Bus&`
and uses it.

## LSP check

`Bme280` only uses the `Bus` contract: `read()` fills exactly `len` bytes from register `reg`
and returns `true`, or returns `false`; `write()` puts `len` bytes on the bus and returns
`true`, or returns `false`; `delayUs()` blocks for at least `us` microseconds. Any
implementation that keeps this contract (`ArduinoI2cBus`, a mock bus in tests, next week's
`LinuxI2cBus`) can be passed as a `Bus&` without the sensor class noticing.

Things that would break substitutability:

* A bus that returns `true` but writes nothing (or reads fewer than `len` bytes). The Bosch
  driver would then work with stale or zeroed calibration data and report believable but wrong
  values, with no error to show for it.
* A `delayUs()` that returns early. The measurement would be read before it is finished.
* A bus that throws, or needs a special call first (e.g. `begin()`) that the sensor does not
  know about.

`ArduinoI2cBus` checks the result of `endTransmission()` and the byte count returned by
`requestFrom()`, and refuses transfers larger than the 32-byte Wire buffer, so it reports
`false` instead of pretending to succeed.

## Verification

What has actually been checked:

* The core (`src/Bme280.cpp`) and the Bosch driver compile on a Linux x86-64 host with
  `g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror`.
* Both example sketches compile and link for `arduino:samd:nano_33_iot` with `arduino-cli`
  1.3.1, ArduinoCore-samd 1.8.14 and arm-none-eabi-gcc 13.2.

Not checked: running on real hardware. No Arduino board and no BME280 were available.
