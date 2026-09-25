/*
 * Stepper Motor Library for ATmega328P + TB6600
 * Header file
 *
 * Week 1 assignment (Prog 5), based on the course starter
 * (Prog5/Code/week1/stepper_library) and student_instructions.md.
 * Pin, mask and motor definitions are the starter's; the state structure,
 * timing macros and the non-blocking API follow the instructions.
 */

#ifndef STEPPER_H
#define STEPPER_H

#include <avr/io.h>
#include <stdbool.h>
#include <stdint.h>

// Pin definitions (ATmega328P, all on PORTD)
#define STEP_PIN    7  // PD7 - TB6600 PUL (Arduino pin 7)
#define DIR_PIN     6  // PD6 - TB6600 DIR (Arduino pin 6)
#define EN_PIN      5  // PD5 - TB6600 ENA (Arduino pin 5)

#define STEP_MASK   (1 << STEP_PIN)
#define DIR_MASK    (1 << DIR_PIN)
#define EN_MASK     (1 << EN_PIN)

// Motor parameters
#define FULL_STEPS_PER_REV  200
#define MICROSTEPS          16
#define STEPS_PER_REV       ((long)FULL_STEPS_PER_REV * MICROSTEPS)

// Timer1: 16 MHz / prescaler 8 = 2 MHz -> 0.5 us per tick, overflow every 32.768 ms
#define TIMER_PRESCALER     8
#define US_TO_TICKS(us)     ((us) * 2UL)
#define MAX_STEP_TICKS      0xFFFFU   // longest interval a 16-bit time difference can hold

// Speed and acceleration
#define DEFAULT_RPM         120.0f    // speed after stepper_init()
#define ACCEL_START_FACTOR  10        // a move starts at 1/10 of the target speed
#define ACCEL_STEP_TICKS    100       // interval change per step while ramping (50 us)

// Direction enum
typedef enum {
    STEPPER_DIR_CW = 0,   // Clockwise (DIR low)
    STEPPER_DIR_CCW = 1   // Counter-clockwise (DIR high)
} stepper_direction_t;

// Complete motor state (FR-007: readable status)
typedef struct {
    long                currentPosition;      // steps, updated after every pulse
    long                targetPosition;       // steps
    stepper_direction_t currentDirection;
    bool                isMoving;
    bool                isEnabled;
    uint16_t            lastStepTime;         // TCNT1 value of the last step
    uint16_t            currentStepInterval;  // ticks, changes while ramping
    uint16_t            targetStepInterval;   // ticks, from the configured speed
} stepper_state_t;

extern volatile stepper_state_t stepper_state;

// --- The six functions of the assignment ---
void     stepper_init(void);
void     stepper_set_direction(stepper_direction_t direction);
void     stepper_move_to(long position);
uint16_t stepper_rpm_to_ticks(float rpm);
void     stepper_make_pulse(void);
void     stepper_update(void);   // call as often as possible, at least every 32 ms

// --- Supporting functions ---
void     stepper_enable(bool enable);
void     stepper_move_by(long steps);        // relative move (FR-003.2)
void     stepper_set_speed(float rpm);       // configurable speed (FR-004.2)
long     stepper_get_position(void);
bool     stepper_is_moving(void);
stepper_direction_t stepper_get_direction(void);
bool     stepper_is_enabled(void);

#endif // STEPPER_H
