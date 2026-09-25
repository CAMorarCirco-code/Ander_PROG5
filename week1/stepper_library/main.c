/*
 * Example usage of the Stepper Motor Library (ATmega328P + TB6600)
 *
 * Ping-pong motion: one revolution clockwise, pause, back to the start and
 * one revolution counter-clockwise, pause, and so on. Every move accelerates
 * from 1/10 of the target speed and decelerates before the target.
 *
 * stepper_update() is non-blocking and is called continuously from the
 * main loop; it decides itself when the next step is due.
 */

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include "stepper.h"

#include <util/delay.h>

int main(void)
{
    stepper_init();                 // pins, Timer1, default speed (120 RPM)
    stepper_set_speed(120.0f);

    long target = STEPS_PER_REV;    // 3200 steps = 1 revolution
    stepper_move_to(target);

    while (1) {
        stepper_update();

        if (!stepper_is_moving()) {
            _delay_ms(500);         // pause at each end
            target = -target;       // ping-pong between +1 and -1 revolution
            stepper_move_to(target);
        }
    }

    return 0;
}
