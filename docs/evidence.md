# Hardware evidence and provenance

This page separates three kinds of result: tests run **against this repository**, hardware results
recorded in **Liv's repository** (Gabrielle Olivia Handoko, `GabrielleOlivia/PROG5_GO.Handoko`)
during the lab session we did together, and tests that are **still missing**.

## Sensor choice

The course overview allows another sensor: "The BME280 is the sensor we use as the concrete
example; a BMP280 or another I2C sensor works just as well"
([`Prog5/Assignments/README.md`](https://github.com/AEAEmbedded/ESE_PROG/blob/911be1bae17dc9bafa61efd6c281d667600658f7/Prog5/Assignments/README.md),
line 5). According to the student, the teacher also confirmed that the alternative setup from the
lab (MPXV5010GP pressure sensor read through an MCP3426 ADC at I2C address `0x68`) is acceptable.

This library implements the BME280. The sensor flexibility means a non-BME280 sensor is not a
problem in itself. It does **not** make another library's output the output of this one: the
hand-ins ask for "Serial output of the example sketch" (week 3), "the terminal output of the
smoke test" (week 4), a `mosquitto_sub` capture of the station (week 5) and "the Pi still
publishes" (week 7), and this repository's code cannot read the MCP3426.

## Liv's recorded results

All files are in `GabrielleOlivia/PROG5_GO.Handoko`, branch `main` at `0ddac17`. They were produced
by **Liv's own library** (`week3/pressurelib`, sensor `MCP3426`), not by this repository's code.

| File | Added in | Date | Platform | Source code that ran | What it shows |
|---|---|---|---|---|---|
| `week3/pressurelib/evidence/arduino-serial-20261013.txt` (procedure: `week3/pressurelib/docs/BOARD_TEST.md`) | `100a7e4` | 13 Oct 2026 | ModularSim SAMD21 SensorHub (Feather M0 Express variant), PlatformIO, COM3 115200 | Liv's `PressureRead` sketch; flashed commit **not recorded**, with a local init-retry change | 85 CSV rows (raw counts, ADC voltage): stable at rest, response to syringe pressure, 7 saturated samples. Pasted excerpts, not a full session |
| `week4/evidence/pi-smoke-test.txt` (procedure: `week4/PI_TEST.md`) | `2aab8f9` | 13 Oct 2026 | Raspberry Pi 4B Rev 1.5, Debian 13 ARM64, kernel `6.18.50+rpt-rpi-v8`, `/dev/i2c-1` | Liv's `pressure_read`, commit `eade45b` | 37 samples at 1 s from `0x68`, no errors, clean Ctrl+C (`script` recording) |
| `week5/evidence/pi-mqtt-real.txt` | `f74b235` | 13 Oct 2026 | same Pi, Mosquitto 2.0.21 | Liv's `pressure_station`, commit `eade45b` | 6 messages: 3 real `0x68` readings on `han/ese/gabrielle-olivia-handoko/mcp3426/state`, 3 from a fake second sensor |
| `week5/evidence/pi-mqtt-simulated.txt` | `f74b235` | 13 Oct 2026 | same Pi | same | 6 messages from two simulated sensors |
| `week7/evidence/pi-ctest.txt`, `pi-week7-doctest.txt` | `0ddac17` | 13 Oct 2026 | same Pi, GCC 14.2.0, CMake 3.31.6 | Liv's commit `1d1cf9f` | Her mock-based tests on ARM64: 3/3 suites, 16/16 doctest cases. No sensor and no broker involved |
| `week5/evidence/mosquitto-sub-fake.txt`, `week6/evidence/*`, `week7/evidence/mosquitto-sub-two-sensors.txt` | various | 8-13 Oct 2026 | x86-64 host | Liv's code | Host runs with simulated data; not hardware evidence |

**What these results establish for this repository.** They are evidence for Liv's library. For
this library they only establish the lab environment:
* the lab Pi 4B has I2C enabled at `/dev/i2c-1`, and a device on the shared module answers at `0x68`;
* Mosquitto 2.0.21 broker and `mosquitto_sub` run on that Pi;
* the Pi's toolchain is GCC 14.2.0 / CMake 3.31.6, and CMake's `FetchContent` can download doctest there.

They do not show that this repository's `ArduinoI2cBus`, `LinuxI2cBus`, `Bme280`, `Sampler` or
`MqttPublisher` work on hardware. None of these files is copied into this repository.

## Run against this repository (software only)

See the verification tables in the [README](../README.md): host GCC 13.3 / 14.2, Clang, CMake 3.28
and 4.4, sanitizers, valgrind, aarch64/armhf under qemu, and an Arduino compile for
`nano_33_iot`. No hardware was used.

## Still missing, and the smallest test that closes each

Run on the lab Pi, from a clone of the `week7` branch. Steps 1-3 need **no BME280**.

| # | Command | Shows | Closes |
|---|---|---|---|
| 1 | `cmake -S . -B build -DBME280_WARNINGS_AS_ERRORS=ON && cmake --build build -j && ctest --test-dir build` (before starting the mosquitto service) | this code builds and passes 9/9 on the real Pi | Week 7 "`ctest` green", natively on the Pi |
| 2 | `sudo systemctl start mosquitto`; `./build/examples/station/station_main --fake &`; `mosquitto_sub -h localhost -t 'han/ese/#' -v -C 6` | sampler thread, both publishers and the broker on the Pi; data from `FakeSensor` | Week 7 "the Pi still publishes" (with simulated values: say so) |
| 3 | `./build/examples/rpi/bme280_rpi_smoke /dev/i2c-1 0x68 1000 1` with the MCP3426 module attached | `LinuxI2cBus` completes a real I2C transaction; expected result `init` fails with `WrongChipId` (an MCP3426 is not a BME280), not with an I2C error | part of Week 4 (real bus). Note: the register-pointer byte `0xD0` is written to the MCP3426's configuration register; power-cycle the module afterwards |
| 4 | with a BME280 module: `i2cdetect -y 1`, `bme280_rpi_smoke /dev/i2c-1 0x76 1000 10`, then `station_main` + `mosquitto_sub` | real readings through this library | Weeks 4 and 5 with real data |
| 5 | with a BME280 on a SAMD21: upload `ReadEverySecond`, Serial Monitor 115200 | Serial output of this example | Week 3 |

Steps 4 and 5 need a BME280 (or BMP280) module. Without one, this library's real-sensor hand-ins
for weeks 3-5 cannot be produced; the alternative is adding a driver for the lab's MCP3426 to this
library, which is a design change (see finding 6 in the [refactor report](refactor_report.md):
`Bus::read` always writes a register pointer, and the MCP3426 has none).
