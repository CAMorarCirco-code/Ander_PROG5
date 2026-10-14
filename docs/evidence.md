# Hardware evidence and provenance

## Shared lab work

**Collaborative hardware measurements and tests.** Ander and Liv (Gabrielle Olivia Handoko) carried
out the Prog5 lab measurements and hardware tests together, on Liv's computer and her physical hardware setup. We chose to use
one working setup rather than duplicate computers and hardware, so the original measurement
records are kept in Liv's repository,
[`GabrielleOlivia/PROG5_GO.Handoko`](https://github.com/GabrielleOlivia/PROG5_GO.Handoko). The
measurements and tests are credited to our shared lab work. That the files are stored in Liv's repository
says where they are kept, not who took part.

**Sensor setup.** An MPXV5010GP pressure sensor read through an MCP3426 ADC at I2C address `0x68`.
The course overview allows another sensor: "The BME280 is the sensor we use as the concrete
example; a BMP280 or another I2C sensor works just as well"
([`Prog5/Assignments/README.md`](https://github.com/AEAEmbedded/ESE_PROG/blob/911be1bae17dc9bafa61efd6c281d667600658f7/Prog5/Assignments/README.md),
line 5). The teacher accepted this setup and these measurements as the hardware evidence for this
lab (as reported by Ander).

**Provenance.** Shared lab work, repository ownership, Git authorship and the code that ran are
different things:

| | Shared lab measurements | This repository |
|---|---|---|
| Who did the work | Ander and Liv, together | Ander (author), with an AI coding assistant |
| Repository owner | Liv | Ander |
| Where the records are | Liv's repository (links below, pinned to commit `0ddac17`) | here |
| Code that ran | **Liv's library** (`week3/pressurelib`), at the commits named per week | this BME280 library |
| Git authorship | commits by `GabrielleOlivia` | commits recorded under the assistant's name, `Claude`, see `git log` |
| Kind of test | real SAMD21 and Raspberry Pi 4B hardware | host builds, mocks, simulated sensors, qemu, Arduino compile |

The readings below were not produced by this repository's BME280 code. The excerpts are quoted
unchanged; the original files are not copied here. The results of this repository's own code are
in the [README](../README.md) verification tables.

## Week 3: Serial output on the SAMD21

| | |
|---|---|
| Evidence | [`week3/pressurelib/evidence/arduino-serial-20261013.txt`](https://github.com/GabrielleOlivia/PROG5_GO.Handoko/blob/0ddac17640ebb75bb9592af15cc73718c073b2ad/week3/pressurelib/evidence/arduino-serial-20261013.txt), procedure and setup in [`week3/pressurelib/docs/BOARD_TEST.md`](https://github.com/GabrielleOlivia/PROG5_GO.Handoko/blob/0ddac17640ebb75bb9592af15cc73718c073b2ad/week3/pressurelib/docs/BOARD_TEST.md) |
| Added in | `100a7e4`, 13 Oct 2026 |
| Platform | ModularSim SAMD21 SensorHub (Feather M0 Express build variant), PlatformIO, COM3 at 115200 baud |
| Code | Liv's `PressureRead` example; the flashed commit was not recorded (stated in the file) |
| Requirement | "Serial output of the example sketch": one measurement per second over Serial |
| Shows | 85 rows at 1 s intervals with no error flag: stable at rest, a clear response to syringe pressure, recovery afterwards |

```text
# t_ms,raw_counts,adc_input_V,at_adc_limit,error
# Resting readings (14 rows)
11602,2518,0.157375,0,0
12602,2517,0.157313,0,0
...
# Manual syringe input / recovery (71 rows)
...
241602,3174,0.198375,0,0
242602,13992,0.874500,0,0
243602,3702,0.231375,0,0
244602,2371,0.148188,0,0
```

## Week 4: smoke test on the Raspberry Pi

| | |
|---|---|
| Evidence | [`week4/evidence/pi-smoke-test.txt`](https://github.com/GabrielleOlivia/PROG5_GO.Handoko/blob/0ddac17640ebb75bb9592af15cc73718c073b2ad/week4/evidence/pi-smoke-test.txt) (unedited `script` recording), procedure in [`week4/PI_TEST.md`](https://github.com/GabrielleOlivia/PROG5_GO.Handoko/blob/0ddac17640ebb75bb9592af15cc73718c073b2ad/week4/PI_TEST.md) |
| Added in | `2aab8f9`, recorded 13 Oct 2026 16:04-16:05 (+02:00) |
| Platform | Raspberry Pi 4B Rev 1.5 (4 GB), Debian 13 Trixie ARM64, kernel `6.18.50+rpt-rpi-v8`, `/dev/i2c-1` |
| Code | Liv's `pressure_read`, her commit `eade45b` |
| Requirement | "the terminal output of the smoke test on the Pi": reads once per second, exits cleanly on Ctrl-C |
| Shows | 37 consecutive samples from the device at `0x68` over Linux `i2c-dev`, no errors, clean Ctrl+C, exit code 0 |

```text
Script started on 2026-10-13 16:04:47+02:00 [COMMAND="./build/pressure_read /dev/i2c-1 0x68" ...]
# MCP3426, CH1, 16-bit, gain x1; raw counts and ADC input voltage
# device=/dev/i2c-1, address=0x68
t_ms,raw_counts,adc_input_V,at_adc_limit,error
101,3637,0.227313,0,0
1101,3637,0.227313,0,0
2101,3639,0.227438,0,0
...
36101,3639,0.227438,0,0
^C# Stopped; I2C connection will close.

Script done on 2026-10-13 16:05:24+02:00 [COMMAND_EXIT_CODE="0"]
```

## Week 5: `mosquitto_sub` capture on the Pi

| | |
|---|---|
| Evidence | [`week5/evidence/pi-mqtt-real.txt`](https://github.com/GabrielleOlivia/PROG5_GO.Handoko/blob/0ddac17640ebb75bb9592af15cc73718c073b2ad/week5/evidence/pi-mqtt-real.txt) and [`pi-mqtt-simulated.txt`](https://github.com/GabrielleOlivia/PROG5_GO.Handoko/blob/0ddac17640ebb75bb9592af15cc73718c073b2ad/week5/evidence/pi-mqtt-simulated.txt); run described in [`week5/README.md`](https://github.com/GabrielleOlivia/PROG5_GO.Handoko/blob/0ddac17640ebb75bb9592af15cc73718c073b2ad/week5/README.md) |
| Added in | `f74b235`, 13 Oct 2026 |
| Platform | same Raspberry Pi 4B, local Mosquitto 2.0.21 broker and `mosquitto_sub` |
| Code | Liv's `pressure_station`, her commit `eade45b` (already with the two-sensor extension) |
| Requirement | sampling thread -> callback -> JSON -> MQTT on the Pi, checked with `mosquitto_sub -t 'han/ese/#' -v` |
| Shows | real sensor readings (`"simulated":false`) and a second, fake sensor published as JSON on their own topics, through a real broker on the Pi |

```text
han/ese/gabrielle-olivia-handoko/mcp3426/state {"raw_counts":3609,"adc_input_V":0.225563,"at_adc_limit":false,"simulated":false}
han/ese/gabrielle-olivia-handoko/mcp3426/simulated/secondary {"raw_counts":10000,"adc_input_V":0.625000,"at_adc_limit":false,"simulated":true}
han/ese/gabrielle-olivia-handoko/mcp3426/state {"raw_counts":3614,"adc_input_V":0.225875,"at_adc_limit":false,"simulated":false}
```

This library's own topic and payload are `han/ese/ander/bme280/state` with
`{"t":…,"p":…,"h":…}`; the README's Week 5 capture shows them, from a host run with `--fake`.

## Week 6

No hardware evidence is required. The design is checked in software (README, Week 6).

## Week 7: tests on the Pi

| | |
|---|---|
| Evidence | [`week7/evidence/pi-ctest.txt`](https://github.com/GabrielleOlivia/PROG5_GO.Handoko/blob/0ddac17640ebb75bb9592af15cc73718c073b2ad/week7/evidence/pi-ctest.txt), [`week7/evidence/pi-week7-doctest.txt`](https://github.com/GabrielleOlivia/PROG5_GO.Handoko/blob/0ddac17640ebb75bb9592af15cc73718c073b2ad/week7/evidence/pi-week7-doctest.txt) |
| Added in | `0ddac17`, 13 Oct 2026 |
| Platform | same Raspberry Pi 4B, Debian 13 ARM64, GCC 14.2.0, CMake 3.31.6, doctest 2.4.11 |
| Code | Liv's commit `1d1cf9f` |
| Requirement | the test suite runs on the target; "the Pi still publishes" is covered by the Week 5 Pi capture above, recorded with Liv's two-sensor station (commit `eade45b`) |
| Shows | 3/3 CTest suites and 16/16 doctest cases (124/124 assertions) passed on ARM64 hardware |

```text
1/3 Test #1: core .............................   Passed    0.01 sec
2/3 Test #2: station ..........................   Passed    0.40 sec
3/3 Test #3: assignment7_unit .................   Passed    0.02 sec

100% tests passed, 0 tests failed out of 3
```

This repository's own Week 7 suite (9 CTest suites, including the doctest `unit` suite) was run
on the host with GCC 13.3 and GCC 14.2 (the Pi's compiler version), CMake 3.28 and 4.4, and
under `qemu-aarch64`; see the README.

## Optional re-run with this repository's code

Not required for the hand-in. On the same Pi, from a clone of this repository:

```bash
cmake -S . -B build -DBME280_WARNINGS_AS_ERRORS=ON && cmake --build build -j && ctest --test-dir build
./build/examples/station/station_main --fake &      # FakeSensor values, real broker
mosquitto_sub -h localhost -t 'han/ese/#' -v -C 6
```
