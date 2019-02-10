
CC=avr-gcc
CFLAGS=-mmcu=attiny88 -Os -std=c11 -g


build: main.c
	$(CC) -o catws.elf $(CFLAGS) main.c
	avr-size catws.elf
	avr-objcopy catws.elf catws.hex -O ihex

.PHONY: flash
flash: build
	avrdude -c usbtiny -p t88 -U flash:w:catws.hex:i -B5

.PHONY: erase
erase:
	avrdude -c usbtiny -p t88 -e -B150
.PHONY: clean

clean:
	rm catws.elf
	rm catws.hex
