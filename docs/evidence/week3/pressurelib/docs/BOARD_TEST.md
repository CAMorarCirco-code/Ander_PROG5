# Week 3 SAMD21 physical test — 13 October 2026

**Status: demonstrated on real hardware.** This verifies communication, acquisition, pressure response and return toward baseline, **not calibrated pressure accuracy**.

## Hardware and firmware

- ModularSim SAMD21 SensorHub (the previously used Sensor A2 setup), SimModule Pressure module with MPXV5010GP and MCP3426 ADC at `0x68`, verified existing 4-wire module cable; hardware photograph supplied by the student.
- PlatformIO `sensorhub_default` on Windows, `atmelsam@9.0.0`, Feather M0 Express build variant. Build succeeded, 23,756 bytes were flashed using COM3, and checksum verification succeeded.
- COM3 USB CDC terminal ran at **115200 baud**.
- The initial upload produced no continuing output, because `sensor.init()` failed during startup and the original sketch returned silently. For the successful test, the student added a **local init retry diagnostic**; subsequent I²C acquisition succeeded. This retry behavior is now committed to the default example, but the specific original startup failure cause is not proven.
- **Exact source commit flashed not recorded**; do not claim the uploaded binary matches a particular Git SHA.

## Actual serial evidence

[arduino-serial-20261013.txt](../evidence/arduino-serial-20261013.txt) contains **85 student-provided CSV readings**, copied from two PlatformIO monitor excerpts; it is not a complete uninterrupted UART capture.

| Check | Observation |
| --- | --- |
| Resting segment | 14 readings, raw counts 2516–2519 |
| Acquisition cadence | Consecutive timestamps separated by 1,000 ms |
| Communication | `error=0` for all 85 shown rows |
| Pressure response | Excursions up to 13,992 and later 32,767 counts |
| Return toward baseline | e.g. 260602 ms = 2522 counts; 275602 ms = 2533 |
| ADC clipping | Seven `32767,2.047938,1,0` readings, including five consecutive seconds |
| Gain x2 custom sketch | Compiled separately, **not physically run** |
| Calibrated kPa / airflow / volume | **Not verified** |
| Full untouched serial session | Not captured; two authentic chat-pasted excerpts retained |

The stiff syringe caused abrupt pressure changes with an airtight seal; avoid forcing it or repeating saturation. A controlled, vented low-pressure source and reference instrument are needed for quantitative calibration, not to prove basic ADC response.

## Repeat-test procedure (only if needed)

1. Record `git rev-parse HEAD` before compiling. With power disconnected, use the known-good Sensor A2 cable; do not improvise power/pin wiring.
2. In `week3/pressurelib`, build and upload `sensorhub_default` using PlatformIO, then monitor COM3 (or the detected port) at 115200 baud; close competing serial monitors.
3. Capture the complete boot banner, CSV header, and at least 10 stable rows. The default sketch now retries initialization every second if the initial attempt fails.
4. Apply only a safely controlled small pressure excursion within sensor and ADC limits, record recovery and flags, and stop rather than force a stiff syringe.
5. Save any future uninterrupted terminal capture separately (e.g. `evidence/arduino-serial-full.txt`) and retain this dated evidence without modifying the values.
6. **Do not use gain x2 with the input that already saturated gain x1.**

The Raspberry Pi direct-I²C smoke test is separate and still pending; see [Week 4's Pi instructions](../../../week4/PI_TEST.md). Historical K.E.V.I.N serial logs are not used as this test's evidence.
