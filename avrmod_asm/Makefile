
NAME := test
HEX := $(NAME).hex
OUT := $(NAME).out
MAP := $(NAME).map

OBJ = sram.o test.o mmc/uart.o mmc/partition.o mmc/fat.o mmc/sd_raw.o mmc/byteordering.o modplay/protracker.o modplay/loader_mod.o modplay/player.o modplay/effects_mod.o modplay/mix.o 

MCU := atmega32
MCU_AVRDUDE := m32
MCU_FREQ := 16000000UL

CC := avr-gcc
OBJCOPY := avr-objcopy
SIZE := avr-size -A
DOXYGEN := doxygen

CFLAGS := -Wall -mmcu=$(MCU) -g -Os -std=c99 -DF_CPU=$(MCU_FREQ)

all: $(HEX)

clean:
	rm -f $(HEX) $(OUT) $(MAP) $(OBJ)
	rm -rf doc/html

flash: $(HEX)
	avrdude -y -c avrispmkII -P usb -p $(MCU_AVRDUDE) -U flash:w:$(HEX)

$(HEX): $(OUT)
	$(OBJCOPY) -R .eeprom -O ihex $< $@

$(OUT): $(OBJ)
	$(CC) $(CFLAGS) -o $@ -Wl,-Map,$(MAP) $^
	@echo
	@$(SIZE) $@
	@echo

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.sx
	$(CC) $(CFLAGS) -c -o $@ $<

%.pp: %.c
	$(CC) $(CFLAGS) -E -o $@ $<

%.ppo: %.c
	$(CC) $(CFLAGS) -E $<

doc: $(HEADERS) $(SOURCES) Doxyfile
	$(DOXYGEN) Doxyfile

.PHONY: all clean flash doc

