/* Host-test stand-in for <avr/io.h>: the registers used by stepper.c are
 * plain variables that the test can inspect and set. */
#ifndef FAKE_AVR_IO_H
#define FAKE_AVR_IO_H
#include <stdint.h>
extern volatile uint8_t  DDRD, PORTD, TCCR1A, TCCR1B;
extern volatile uint16_t TCNT1;
#define CS10 0
#define CS11 1
#define CS12 2
#endif
