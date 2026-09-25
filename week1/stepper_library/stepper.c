/*
 * Stepper Motor Library for ATmega328P + TB6600
 * Implementation file
 *
 * Week 1 assignment (Prog 5): the six functions from student_instructions.md
 * (stepper_init, stepper_set_direction, stepper_move_to, stepper_rpm_to_ticks,
 * stepper_make_pulse, stepper_update) plus a few small helpers.
 *
 * Timing: Timer1 runs free at 2 MHz (0.5 us per tick). stepper_update() polls
 * TCNT1 and uses uint16_t differences, so one timer overflow between two
 * calls is harmless. It must be called at least every 32.768 ms.
 */

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include "stepper.h"

#include <avr/interrupt.h>
#include <util/delay.h>

volatile stepper_state_t stepper_state;

// Ramp bookkeeping (internal): the interval a move starts with, and how many
// acceleration steps were taken, so deceleration can mirror them.
static uint16_t startStepInterval;
static uint16_t rampSteps;

// ------------------------------------------------------------ 1. init --

void stepper_init(void)
{
    // STEP, DIR and EN as outputs
    DDRD |= (STEP_MASK | DIR_MASK | EN_MASK);

    // Initial pin states: STEP low, DIR low, EN high (driver enabled)
    PORTD &= ~STEP_MASK;
    PORTD &= ~DIR_MASK;
    PORTD |= EN_MASK;

    // Timer1: normal mode (WGM1x = 0), prescaler 8 (CS11) -> 2 MHz
    TCCR1A = 0x00;
    TCCR1B = (1 << CS11);
    TCNT1  = 0;

    sei();

    stepper_state.currentPosition  = 0;
    stepper_state.targetPosition   = 0;
    stepper_state.currentDirection = STEPPER_DIR_CW;
    stepper_state.isMoving         = false;
    stepper_state.isEnabled        = true;
    stepper_state.lastStepTime     = 0;

    // Default speed
    stepper_set_speed(DEFAULT_RPM);
    stepper_state.currentStepInterval = stepper_state.targetStepInterval;
}

// ------------------------------------------------------- 2. direction --

void stepper_set_direction(stepper_direction_t direction)
{
    if (direction == STEPPER_DIR_CW) {
        PORTD &= ~DIR_MASK;   // DIR low = clockwise
    } else {
        PORTD |= DIR_MASK;    // DIR high = counter-clockwise
    }
    _delay_us(20);            // TB6600 direction setup time (>= 20 us)
    stepper_state.currentDirection = direction;
}

// --------------------------------------------------------- 3. move_to --

void stepper_move_to(long position)
{
    stepper_state.targetPosition = position;

    if (position == stepper_state.currentPosition) {
        stepper_state.isMoving = false;
        return;
    }

    stepper_set_direction(position > stepper_state.currentPosition ? STEPPER_DIR_CW
                                                                   : STEPPER_DIR_CCW);

    // Every move starts slow and accelerates (see stepper_update)
    uint32_t start = (uint32_t)stepper_state.targetStepInterval * ACCEL_START_FACTOR;
    startStepInterval = (start > MAX_STEP_TICKS) ? MAX_STEP_TICKS : (uint16_t)start;
    stepper_state.currentStepInterval = startStepInterval;
    rampSteps = 0;

    stepper_state.lastStepTime = TCNT1;
    stepper_state.isMoving = true;
}

// ---------------------------------------------------- 4. rpm_to_ticks --

uint16_t stepper_rpm_to_ticks(float rpm)
{
    // RPM -> steps per second
    float steps_per_second = (rpm / 60.0f) * STEPS_PER_REV;
    if (steps_per_second < 1.0f) {
        return MAX_STEP_TICKS;        // also catches rpm <= 0: slowest possible
    }

    // steps per second -> microseconds per step -> timer ticks
    uint32_t step_interval_us    = 1000000UL / (uint32_t)steps_per_second;
    uint32_t step_interval_ticks = US_TO_TICKS(step_interval_us);

    if (step_interval_ticks > MAX_STEP_TICKS) {
        return MAX_STEP_TICKS;        // slower than one step per 32.7 ms
    }
    if (step_interval_ticks == 0) {
        return 1;
    }
    return (uint16_t)step_interval_ticks;
}

// ------------------------------------------------------ 5. make_pulse --

void stepper_make_pulse(void)
{
    PORTD |= STEP_MASK;    // STEP high (TB6600 steps on the rising edge)
    _delay_us(5);          // 80 cycles at 16 MHz
    PORTD &= ~STEP_MASK;   // STEP low
    _delay_us(5);
}

// ---------------------------------------------------------- 6. update --

void stepper_update(void)
{
    if (!stepper_state.isEnabled || !stepper_state.isMoving) {
        return;
    }

    uint16_t currentTime = TCNT1;
    if ((uint16_t)(currentTime - stepper_state.lastStepTime) < stepper_state.currentStepInterval) {
        return;   // not time for the next step yet
    }

    if (stepper_state.currentPosition != stepper_state.targetPosition) {
        stepper_make_pulse();
        stepper_state.lastStepTime = currentTime;

        if (stepper_state.currentDirection == STEPPER_DIR_CW) {
            stepper_state.currentPosition++;
        } else {
            stepper_state.currentPosition--;
        }

        // Linear ramp: accelerate towards the target interval, and start
        // decelerating when the steps left equal the steps used to accelerate.
        long remaining = stepper_state.targetPosition - stepper_state.currentPosition;
        if (remaining < 0) {
            remaining = -remaining;
        }
        uint16_t interval = stepper_state.currentStepInterval;
        uint16_t target   = stepper_state.targetStepInterval;

        if (rampSteps > 0 && remaining <= (long)rampSteps) {
            // Decelerate (increase interval), never slower than the start speed
            uint32_t slower = (uint32_t)interval + ACCEL_STEP_TICKS;
            interval = (slower > startStepInterval) ? startStepInterval : (uint16_t)slower;
            rampSteps--;
        } else if (interval > target) {
            // Accelerate (decrease interval), limited to the target speed
            if (interval - target > ACCEL_STEP_TICKS) {
                interval -= ACCEL_STEP_TICKS;
            } else {
                interval = target;
            }
            rampSteps++;
        } else if (interval < target) {
            interval = target;   // speed was lowered during the move
        }
        stepper_state.currentStepInterval = interval;
    }

    if (stepper_state.currentPosition == stepper_state.targetPosition) {
        stepper_state.isMoving = false;
    }
}

// -------------------------------------------------------------- helpers --

void stepper_enable(bool enable)
{
    if (enable) {
        PORTD |= EN_MASK;    // EN high = driver enabled
    } else {
        PORTD &= ~EN_MASK;   // EN low = driver disabled
    }
    _delay_us(20);           // TB6600 setup time
    stepper_state.isEnabled = enable;
    stepper_state.lastStepTime = TCNT1;   // do not step immediately after enabling
}

void stepper_move_by(long steps)
{
    stepper_move_to(stepper_state.currentPosition + steps);
}

void stepper_set_speed(float rpm)
{
    stepper_state.targetStepInterval = stepper_rpm_to_ticks(rpm);
}

long stepper_get_position(void)
{
    return stepper_state.currentPosition;
}

bool stepper_is_moving(void)
{
    return stepper_state.isMoving;
}

stepper_direction_t stepper_get_direction(void)
{
    return stepper_state.currentDirection;
}

bool stepper_is_enabled(void)
{
    return stepper_state.isEnabled;
}
