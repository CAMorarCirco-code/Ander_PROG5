/* Host-test stand-in for <util/delay.h>: records each delay and the PORTD
 * value at that moment instead of waiting. */
#ifndef FAKE_UTIL_DELAY_H
#define FAKE_UTIL_DELAY_H
void _delay_us(double us);
void _delay_ms(double ms);
#endif
