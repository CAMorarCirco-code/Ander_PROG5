# Raspberry Pi build and physical smoke test

**Status: physical Raspberry Pi/direct-I²C smoke test passed on 13 October 2026.** This page records the actual test and retains the repeatable procedure. Earlier K.E.V.I.N USB-serial logs remain historical evidence.

## Hardware prerequisite

Confirm the actual module's connector pinout, supply and I2C pull-up voltage before making a direct Pi connection. K.E.V.I.N documents a mismatch between an older schematic and the fitted sensor; the proven SensorHub cable does not establish a Pi-compatible connection. The signals must be compatible with the Pi's 3.3 V GPIO. If those module details are uncertain, arrange the verified test connection with the lab supervisor. Make connections with power off; the SAMD21 must not drive the same bus during this single-controller test.

## Prepare Raspberry Pi OS

Update the existing PROG5 clone with `git pull --ff-only`, or clone it using your own GitHub credentials. Install tools and enable I2C:

```sh
sudo apt update
sudo apt install build-essential cmake python3 i2c-tools
sudo raspi-config
```

Enable I2C under Interface Options and reboot if requested. Check the adapter and this ADC's address:

```sh
i2cdetect -l
ls -l /dev/i2c-1
i2cdetect -y 1 0x68 0x68
```

The limited scan should show 68. If another adapter is used, change the bus number and device path consistently. Resolve wiring, power, bus selection and access errors before continuing. A scan response does not establish calibrated pressure.

If the device belongs to the i2c group and your account lacks access, add the current user and then log out and back in:

```sh
sudo usermod -aG i2c "$USER"
```

Check membership with `groups`; run the example as the normal user once access is configured.

## Build and capture

From the repository root:

```sh
cd week3/pressurelib
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Save actual Pi build results separately if desired. From the same folder, record the hardware terminal session:

```sh
script -q -c './build/pressure_read /dev/i2c-1 0x68' ../../week4/evidence/pi-smoke-test.txt
```

Capture at least 10 successful readings at rest. If separately testing a pressure change and recovery, confirm the sensor's analog power characteristics and use only gentle pressure; do not force the stiff syringe. Pressure response was not tested on the Pi in the 13 October capture. Press Ctrl+C and confirm the normal stop message. The script command records the session; keep its raw output unchanged, including errors.

Successful rows have error=0 and approximately 1000 ms spacing. No exact resting raw value is prescribed. Blank measurement fields with a nonzero error mean a rejected sample; investigate repeated errors or ADC-limit flags before declaring success.

## Actual test record

| Field | Result |
| --- | --- |
| Date / tested source | 13 October 2026; commit `eade45b2d7921e7b318449de1f5bcf9afe950596` transferred to Pi by Git archive |
| Pi model / OS | Raspberry Pi 4B Rev 1.5, 4 GB; Debian 13 Trixie, ARM64; kernel `6.18.50+rpt-rpi-v8` |
| Compiler / CMake | GNU C++ 14.2.0; CMake 3.31.6 |
| Module / verified wiring | SimModule Pressure v2.0.0; labelled GND/3.3V/SCL/SDA to Pi physical pins 6/1/5/3; GND continuity checked; 3.30 V on labelled supply/SCL/SDA measured on the SensorHub before disconnecting it |
| I²C adapter and detection | `/dev/i2c-1`, `bcm2835`; restricted scan at `0x68` acknowledged `68` |
| CMake / automated tests | ARM64 Pi build passed; **3/3 CTest suites passed** in 1.24 seconds |
| Physical ADC acquisition | **37/37 resting rows** approximately 1 s apart; raw 3636–3643, voltage 0.227250–0.227688 V; error=0, at_adc_limit=0 for all rows |
| Pressure response and calibration | Not verified on the Pi; nominal 5 V sensor component versus measured 3.3 V module connector requires separate evaluation |
| Shutdown | Ctrl+C printed normal stop; `script` reported exit code 0 |
| Raw evidence | [pi-smoke-test.txt](evidence/pi-smoke-test.txt), captured 13 October at 16:05 +02:00 |

The [Week 4 report](README.md#physical-raspberry-pi-smoke-test--13-october-2026) includes a selected excerpt. The original `script` recording should be committed unchanged, then tag the reviewed submission as `v0.4-pi`. Host-simulated output is not hardware evidence.

Reference: [Raspberry Pi configuration documentation](https://www.raspberrypi.com/documentation/computers/configuration.html).
