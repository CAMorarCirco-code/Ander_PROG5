/*
 * Host unit tests for stepper.c (logic only). stepper.c is compiled
 * unchanged against the fake AVR headers in fake/.
 *
 *   cc -std=c99 -Wall -Wextra -Ifake -I../../stepper_library \
 *      test_stepper.c ../../stepper_library/stepper.c -o test_stepper && ./test_stepper
 */
#include "stepper.h"

#include <stdio.h>
#include <stdlib.h>

volatile uint8_t  DDRD, PORTD, TCCR1A, TCCR1B;
volatile uint16_t TCNT1;
int fake_sei_called;

#define MAX_DELAYS 64
static struct { double us; uint8_t portd; } delays[MAX_DELAYS];
static int n_delays;
void _delay_us(double us) { if (n_delays < MAX_DELAYS) { delays[n_delays].us = us; delays[n_delays].portd = PORTD; } n_delays++; }
void _delay_ms(double ms) { _delay_us(ms * 1000.0); }

static int failures, checks;
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void reset_hw(void)
{
    DDRD = 0; PORTD = 0; TCCR1A = 0xFF; TCCR1B = 0; TCNT1 = 1234;
    fake_sei_called = 0; n_delays = 0;
}

/* Let the timer advance exactly to the next due step and call update. */
static int step_once(void)
{
    long before = stepper_get_position();
    TCNT1 = (uint16_t)(stepper_state.lastStepTime + stepper_state.currentStepInterval);
    stepper_update();
    return stepper_get_position() != before;
}

static void test_init(void)
{
    printf("stepper_init\n");
    reset_hw();
    PORTD = STEP_MASK | DIR_MASK;   // garbage that init must clear
    stepper_init();
    CHECK((DDRD & (STEP_MASK | DIR_MASK | EN_MASK)) == (STEP_MASK | DIR_MASK | EN_MASK));
    CHECK((PORTD & STEP_MASK) == 0);
    CHECK((PORTD & DIR_MASK) == 0);
    CHECK((PORTD & EN_MASK) == EN_MASK);
    CHECK(TCCR1A == 0x00);                  // normal mode
    CHECK(TCCR1B == 0x02);                  // prescaler 8
    CHECK(TCNT1 == 0);
    CHECK(fake_sei_called);
    CHECK(stepper_state.targetStepInterval == 312);   // default 120 RPM
    CHECK(stepper_is_enabled());
    CHECK(!stepper_is_moving());
    CHECK(stepper_get_position() == 0);
    CHECK(stepper_get_direction() == STEPPER_DIR_CW);
}

static void test_rpm_to_ticks(void)
{
    printf("stepper_rpm_to_ticks\n");
    CHECK(stepper_rpm_to_ticks(120.0f) == 312);   // 6400 sps -> 156 us -> 312 ticks
    CHECK(stepper_rpm_to_ticks(60.0f) == 624);    // 3200 sps -> 312 us
    CHECK(stepper_rpm_to_ticks(1.0f) == 37734);   // 53.33 -> 53 sps -> 18867 us
    CHECK(stepper_rpm_to_ticks(0.0f) == 0xFFFF);
    CHECK(stepper_rpm_to_ticks(-5.0f) == 0xFFFF);
    CHECK(stepper_rpm_to_ticks(0.1f) == 0xFFFF);  // needs > 16 bits: clamped
    CHECK(stepper_rpm_to_ticks(100000.0f) == 1);  // absurdly fast: never 0
}

static void test_direction(void)
{
    printf("stepper_set_direction\n");
    reset_hw(); stepper_init(); n_delays = 0;
    stepper_set_direction(STEPPER_DIR_CCW);
    CHECK(PORTD & DIR_MASK);
    CHECK(stepper_get_direction() == STEPPER_DIR_CCW);
    CHECK(n_delays == 1 && delays[0].us >= 20.0);
    stepper_set_direction(STEPPER_DIR_CW);
    CHECK((PORTD & DIR_MASK) == 0);
    CHECK(stepper_get_direction() == STEPPER_DIR_CW);
    CHECK((PORTD & EN_MASK) && (PORTD & STEP_MASK) == 0);   // other pins untouched
}

static void test_make_pulse(void)
{
    printf("stepper_make_pulse\n");
    reset_hw(); stepper_init(); n_delays = 0;
    stepper_make_pulse();
    CHECK(n_delays == 2);
    CHECK(delays[0].us == 5.0 && (delays[0].portd & STEP_MASK));        // 5 us HIGH
    CHECK(delays[1].us == 5.0 && (delays[1].portd & STEP_MASK) == 0);   // 5 us LOW
    CHECK((PORTD & STEP_MASK) == 0);
}

static void test_move_to(void)
{
    printf("stepper_move_to\n");
    reset_hw(); stepper_init();
    stepper_move_to(100);
    CHECK(stepper_is_moving());
    CHECK(stepper_get_direction() == STEPPER_DIR_CW);
    CHECK(stepper_state.targetPosition == 100);
    CHECK(stepper_state.currentStepInterval == 312 * ACCEL_START_FACTOR);   // starts slow

    stepper_move_to(-100);
    CHECK(stepper_get_direction() == STEPPER_DIR_CCW);
    CHECK(PORTD & DIR_MASK);

    stepper_move_to(0);   // already there
    CHECK(!stepper_is_moving());
}

static void test_update_timing(void)
{
    printf("stepper_update: timing and overflow\n");
    reset_hw(); stepper_init();
    stepper_move_to(10);
    uint16_t interval = stepper_state.currentStepInterval;

    TCNT1 = (uint16_t)(stepper_state.lastStepTime + interval - 1);
    stepper_update();
    CHECK(stepper_get_position() == 0);          // one tick too early: no step
    TCNT1++;
    stepper_update();
    CHECK(stepper_get_position() == 1);          // due: exactly one step
    CHECK(stepper_state.lastStepTime == TCNT1);  // lastStepTime updated
    stepper_update();
    CHECK(stepper_get_position() == 1);          // same time again: no second step

    // Timer overflow between two steps: 0xFF00 -> 0x0000 wraps
    stepper_state.lastStepTime = 0xFF00;
    TCNT1 = (uint16_t)(0xFF00 + stepper_state.currentStepInterval);   // wraps past 0
    CHECK(TCNT1 < 0xFF00);
    stepper_update();
    CHECK(stepper_get_position() == 2);
}

static void test_update_disabled(void)
{
    printf("stepper_update: disabled driver does not step\n");
    reset_hw(); stepper_init();
    stepper_move_to(5);
    stepper_enable(false);
    CHECK((PORTD & EN_MASK) == 0);
    for (int i = 0; i < 10; i++) step_once();
    CHECK(stepper_get_position() == 0);
    CHECK(stepper_is_moving());                  // move is paused, not lost
    stepper_enable(true);
    while (stepper_is_moving()) step_once();
    CHECK(stepper_get_position() == 5);
}

static void test_full_move_and_ramp(void)
{
    printf("stepper_update: full move, acceleration and deceleration\n");
    reset_hw(); stepper_init();
    const long target = STEPS_PER_REV;           // 3200 steps
    stepper_move_to(target);

    static uint16_t intervals[3300];
    long n = 0;
    int guard = 0;
    while (stepper_is_moving() && guard++ < 10000) {
        intervals[n] = stepper_state.currentStepInterval;   // interval before this step
        if (step_once()) n++;
    }
    CHECK(stepper_get_position() == target);     // exact, +-0 steps
    CHECK(n == target);
    CHECK(!stepper_is_moving());
    CHECK(intervals[0] == 3120);                 // start at 1/10 speed
    CHECK(intervals[n / 2] == 312);              // cruising at 120 RPM
    CHECK(intervals[n - 1] > 2000);              // slowed down again at the end

    long accel = 0, decel = 0;
    for (long i = 1; i < n; i++) {
        if (intervals[i] < intervals[i - 1]) accel++;
        if (intervals[i] > intervals[i - 1]) decel++;
        CHECK(intervals[i] >= 312 && intervals[i] <= 3120);
    }
    CHECK(accel > 0 && accel == decel);          // symmetric ramp
    printf("  %ld steps, %ld accelerating, %ld decelerating\n", n, accel, decel);

    // no further steps once the target is reached
    step_once();
    CHECK(stepper_get_position() == target);
}

static void test_short_move_and_relative(void)
{
    printf("stepper_move_by / short move\n");
    reset_hw(); stepper_init();
    stepper_move_by(7);
    while (stepper_is_moving()) step_once();
    CHECK(stepper_get_position() == 7);
    stepper_move_by(-10);
    CHECK(stepper_get_direction() == STEPPER_DIR_CCW);
    while (stepper_is_moving()) step_once();
    CHECK(stepper_get_position() == -3);
}

static void test_ping_pong(void)
{
    printf("ping-pong position accuracy over 20 moves\n");
    reset_hw(); stepper_init();
    long target = STEPS_PER_REV;
    for (int i = 0; i < 20; i++) {
        stepper_move_to(target);
        while (stepper_is_moving()) step_once();
        CHECK(stepper_get_position() == target);
        target = -target;
    }
}

static void test_speed_change(void)
{
    printf("stepper_set_speed\n");
    reset_hw(); stepper_init();
    stepper_set_speed(60.0f);
    CHECK(stepper_state.targetStepInterval == 624);
    stepper_move_to(2000);
    for (int i = 0; i < 1000; i++) step_once();
    CHECK(stepper_state.currentStepInterval == 624);
    stepper_set_speed(30.0f);                    // slower during the move
    step_once();
    CHECK(stepper_state.currentStepInterval >= 624);
    while (stepper_is_moving()) step_once();
    CHECK(stepper_get_position() == 2000);
}

int main(void)
{
    test_init();
    test_rpm_to_ticks();
    test_direction();
    test_make_pulse();
    test_move_to();
    test_update_timing();
    test_update_disabled();
    test_full_move_and_ramp();
    test_short_move_and_relative();
    test_ping_pong();
    test_speed_change();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
