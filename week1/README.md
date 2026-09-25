# Week 1 - Stepper motor library (ATmega328P + TB6600)

Prog 5 warm-up assignment: complete the stepper motor library described in the course's
`Prog5/Code/week1/Assignment/student_instructions.md`, meeting
`Prog5/Code/week1/Assignment/requirements_list.md`. Plain C, register level, no Arduino core.

```
stepper_library/
  stepper.h      pins, motor parameters, Timer1 macros, stepper_state_t, prototypes
  stepper.c      the six assignment functions + small helpers
  main.c         ping-pong demo (+1 rev, pause, -1 rev, pause, ...)
  Makefile       avr-gcc build, `make flash` via avrdude
verification/
  host_test/     unit tests: stepper.c compiled unchanged against fake AVR registers
  sim_timing.c   runs the real firmware in the simavr ATmega328P simulator, measures the pins
  Makefile       `make host-test`, `make sim`
```

## What was implemented

| Function | What it does |
| --- | --- |
| `stepper_init()` | STEP/DIR/EN as outputs in `DDRD`; STEP low, DIR low, EN high; Timer1 normal mode (`TCCR1A = 0`), prescaler 8 (`TCCR1B = 1 << CS11`), `TCNT1 = 0`; `sei()`; default speed 120 RPM |
| `stepper_set_direction()` | DIR low = CW, DIR high = CCW in `PORTD`; 20 us setup delay; stores the direction |
| `stepper_move_to()` | sets the target, picks CW if the target is larger, else CCW, calls `stepper_set_direction()`, sets `isMoving`, starts the speed ramp at 1/10 of the target speed |
| `stepper_rpm_to_ticks()` | RPM -> steps/s -> us per step -> Timer1 ticks (`US_TO_TICKS`); rpm <= 0 or too slow gives 65535, never returns 0 |
| `stepper_make_pulse()` | STEP high, 5 us, STEP low, 5 us |
| `stepper_update()` | non-blocking: reads `TCNT1`; if `(uint16_t)(now - lastStepTime) >= currentStepInterval`, the driver is enabled and the target is not reached: pulse, update `lastStepTime` and the position, then adjust the speed; clears `isMoving` at the target |

Helpers: `stepper_enable()` (EN pin, as in the starter), `stepper_move_by()` (relative move),
`stepper_set_speed()`, and status getters `stepper_get_position()`, `stepper_is_moving()`,
`stepper_get_direction()`, `stepper_is_enabled()`. The complete state is in the global
`stepper_state` (a `stepper_state_t`).

**Acceleration and deceleration.** A move starts with an interval 10x the target interval.
After every step the interval shrinks by `ACCEL_STEP_TICKS` (100 ticks = 50 us), the rule from
the instructions, until it reaches the target interval. The number of accelerating steps is
counted; when the remaining distance equals that count, the interval grows again by the same
amount per step. The result is a symmetric linear-in-interval ramp that also works for short
moves (they turn around halfway). If the speed is lowered during a move, the interval simply
jumps to the new, slower value.

## Build

```bash
cd stepper_library
make            # stepper.elf, stepper.hex (avr-gcc, ATmega328P, 16 MHz)
make size
make flash PORT=/dev/ttyUSB0    # Arduino Uno/Nano bootloader via avrdude
```

Wiring: use the **active-high** variant from note 2 of the course's
`stepper_wiring_schematic.svg`: D7/PD7 -> PUL+, D6/PD6 -> DIR+, D5/PD5 -> ENA+, Arduino GND ->
PUL-/DIR-/ENA-. Then EN HIGH = driver enabled, which is what `requirements_list.md` asks for
(FR-001.2: STEP=LOW, DIR=LOW, EN=HIGH). The schematic's drawn default is the active-low
variant (5V -> "+" inputs, pins -> "-" inputs, ENA LOW = enabled); with that wiring the
EN levels in this code would be inverted. TB6600 DIP switches SW1-3 set to 1/16 microsteps
(3200 steps/rev), SW4-6 to the motor's rated current.

## Requirements coverage (`requirements_list.md`)

| Requirement | Where / how |
| --- | --- |
| FR-001.1-5 init: outputs, initial states, Timer1 normal/prescaler 8, `sei()`, default speed | `stepper_init()`; host test `test_init` |
| FR-002.1-4 direction: CW/CCW, DIR pin, 20 us delay, state | `stepper_set_direction()`; host test `test_direction`; 320-cycle delay in the disassembly |
| FR-003.1 absolute move, FR-003.3 automatic direction | `stepper_move_to()`; `test_move_to` |
| FR-003.2 relative move | `stepper_move_by()`; `test_short_move_and_relative` |
| FR-003.4-5 position tracking, status flags | `stepper_update()`; `test_full_move_and_ramp`, `test_ping_pong` |
| FR-004.1-4 RPM -> ticks, configurable speed | `stepper_rpm_to_ticks()`, `stepper_set_speed()`; `test_rpm_to_ticks`, `test_speed_change` |
| FR-005.1-4 5 us high / 5 us low pulse | `stepper_make_pulse()` with `_delay_us(5)`; `test_make_pulse`; simavr: 5.125 us high |
| FR-006.1-6 polling loop, step timing, position, accel/decel, target detection, status | `stepper_update()`; host tests + simavr ramp measurement |
| FR-007.1-4 position, moving, direction, enable status | getters + `stepper_state`; checked in several tests |
| NFR-001.1 / NFR-002.4 0.5 us resolution, overflow every 32.768 ms | Timer1 at 2 MHz; `uint16_t` differences; `test_update_timing` wraps `0xFF00 -> 0x0000` |
| NFR-001.3 / NFR-002.3 direction setup >= 20 us | `_delay_us(20)` = 320 cycles |
| NFR-002.1-2 pulse 5 us +-1 us | simavr: STEP high 5.125 us on every pulse |
| NFR-003.1 / NFR-004.2 position +-1 step, stays accurate | host test: exact positions after 20 ping-pong moves of 3200 steps |
| NFR-004.1 overflow handling | as above; `stepper_update()` must run at least every 32.768 ms (documented in `stepper.h`) |
| HWR-004 pins PD7/PD6/PD5 on PORTD | `stepper.h` (same as the starter) |
| SWR-001/002/003 avr-gcc, C99, avr/io.h, avr/interrupt.h, util/delay.h, header + implementation, register-level bit manipulation | `Makefile` uses `-std=c99`; only those three AVR headers |
| IF-002.4 example usage | `main.c` |
| VT-003.2 ping-pong demonstration | `main.c`, simulated in simavr |
| Common mistake "not checking `isEnabled`" | `stepper_update()` returns early; `test_update_disabled`; simavr: 0 steps while EN low |

## Verification performed (no hardware)

* `make` with avr-gcc 7.3.0, `-std=c99 -Os -Wall -Wextra -Werror`: builds, 2192 bytes flash,
  22 bytes RAM.
* `make -C verification host-test`: 11 test groups, 3281 checks, 0 failures (also clean under
  AddressSanitizer/UBSan). A deliberately shortened direction delay made the tests fail, so
  they do detect errors.
* `make -C verification sim`: the real `stepper.elf` in simavr 1.6 (ATmega328P, 16 MHz),
  3.5 s simulated:
  * STEP high time 5.125 us on every pulse (required 5 +-1 us);
  * move 1: 3200 steps CW, move 2: 6400 steps CCW (+1 rev to -1 rev), positions -3200 .. 3200;
  * step interval ramps from about 1515 us up to 156 us (120 RPM = 6400 steps/s) and back to
    about 1562 us before each target;
  * no steps while EN is low; first step comes 1.59 ms after a DIR change (>= 20 us).
* Disassembly: `stepper_make_pulse` holds STEP high for 80 cycles (5.0 us) plus the `sbi`,
  `stepper_set_direction` waits 320 cycles (20.0 us).

## Not verified (needs hardware)

* Nothing has been run on a real ATmega328P, TB6600 or motor. Motor movement, smooth
  running, missed steps, torque and driver behaviour are untested (EDU-003.2/3, VT-002.1,
  VT-003.1/3, VT-004).
* Pulse and delay timing were measured in a simulator, not with an oscilloscope.
* EN polarity: the code assumes the active-high wiring above (EN HIGH = enabled), as in the
  starter and the requirements. It has not been checked against a real TB6600 board.

## Notes on the teacher material

* `student_instructions.md` calls `stepper.h` "complete" and refers to `stepper_state`,
  `US_TO_TICKS`, `isEnabled` and `stepper_update()`, but the `stepper.h` in
  `Prog5/Code/week1/stepper_library/` has none of these (it is a blocking library with
  `stepper_step()`/`stepper_move_steps()`). So `stepper.h` here keeps the starter's pin,
  mask, motor and direction definitions and adds the state structure, the Timer1 macros
  (`US_TO_TICKS` as in `stepper_code_example_1a`) and the prototypes the instructions ask for.
* The instructions suggest "80 nop instructions" for 5 us. A `for` loop of 80 `nop`s costs
  about 4-5 cycles per iteration, i.e. roughly 20-25 us, so `_delay_us()` from
  `<util/delay.h>` (listed in SWR-001.3) is used: it produces exact cycle counts at 16 MHz.
* The bonus challenges (S-curve, Timer1 compare interrupt, multiple motors, encoder, UART)
  were not done.
