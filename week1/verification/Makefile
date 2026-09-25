# Verification without hardware (Linux host).
#   make host-test  -> unit tests of stepper.c against fake AVR registers
#   make sim        -> run the real AVR firmware in simavr and check pin timing
# Needs: gcc; for `sim` also avr-gcc, avr-libc, libsimavr-dev, libelf-dev.

LIB = ../stepper_library

all: host-test sim

host_test/test_stepper: host_test/test_stepper.c $(LIB)/stepper.c $(LIB)/stepper.h
	cc -std=c99 -Wall -Wextra -Werror -Ihost_test/fake -I$(LIB) \
	   host_test/test_stepper.c $(LIB)/stepper.c -o $@

host-test: host_test/test_stepper
	./host_test/test_stepper

sim_timing: sim_timing.c
	cc -O2 -Wall -Wextra sim_timing.c -lsimavr -lelf -o $@

sim: sim_timing
	$(MAKE) -C $(LIB) stepper.elf
	./sim_timing $(LIB)/stepper.elf 3.5

clean:
	rm -f host_test/test_stepper sim_timing

.PHONY: all host-test sim clean
