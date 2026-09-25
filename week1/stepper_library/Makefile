# Build for ATmega328P (Arduino Uno/Nano) at 16 MHz with avr-gcc.
#   make          -> stepper.elf + stepper.hex
#   make flash    -> upload with avrdude (Arduino bootloader, PORT=/dev/ttyUSB0)
#   make size     -> flash/RAM usage

MCU     = atmega328p
F_CPU   = 16000000UL
CC      = avr-gcc
OBJCOPY = avr-objcopy
CFLAGS  = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -std=c99 -Os -Wall -Wextra
PORT   ?= /dev/ttyUSB0

all: stepper.hex

stepper.elf: main.c stepper.c stepper.h
	$(CC) $(CFLAGS) main.c stepper.c -o $@

stepper.hex: stepper.elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

size: stepper.elf
	avr-size --mcu=$(MCU) -C $<

flash: stepper.hex
	avrdude -c arduino -p m328p -P $(PORT) -b 115200 -U flash:w:$<:i

clean:
	rm -f stepper.elf stepper.hex

.PHONY: all size flash clean
