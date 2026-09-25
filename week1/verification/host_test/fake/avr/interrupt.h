#ifndef FAKE_AVR_INTERRUPT_H
#define FAKE_AVR_INTERRUPT_H
extern int fake_sei_called;
#define sei() (fake_sei_called = 1)
#endif
